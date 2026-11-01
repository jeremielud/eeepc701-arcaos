/*
 * eeekeys : touches Fn de l'Eee PC 701 sous ArcaOS.
 *
 * Les touches Fn de l'Eee PC ne passent pas par le clavier : le BIOS les signale
 * par des notifications ACPI du périphérique « ASUS010 » (\_SB.ATKD). Ce démon
 * attend ces notifications grâce au kit ACPI d'Arca Noae et agit :
 *
 *   0x13  Fn+F10  sourdine (bascule)
 *   0x14  Fn+F11  volume -
 *   0x15  Fn+F12  volume +
 *   0x20..0x2F    Fn+F3/F4 luminosité : le BIOS la règle déjà lui-même ; le niveau
 *                 annoncé est réappliqué par la méthode PBLS, au cas où.
 *   0x12  Fn+F9   liste des fenêtres (comme Ctrl+Échap)
 *
 * Le Wi-Fi (Fn+F2) et l'écran externe (Fn+F8) restent au BIOS : à l'initialisation
 * (méthode INIT), aucune fonction ne lui est retirée.
 *
 * Le volume est celui du mixage général de MMPM/2 (commande MCI « masteraudio »),
 * qui pilote Uniaud. Les notifications inconnues sont notées dans le journal
 * %ETC%\..\LOG\EEEKEYS.LOG (C:\VAR\LOG\EEEKEYS.LOG) pour pouvoir les ajouter.
 *
 * Construction (OpenWatcom, depuis Linux ou OS/2) : voir le makefile.
 */
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#include <os2.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "acpi.h"
#include "acpiapi.h"

#define EEE_HID         "ASUS010"
#define NOTIFY_WLAN     0x10
#define NOTIFY_TASKS    0x12
#define NOTIFY_MUTE     0x13
#define NOTIFY_VOLDOWN  0x14
#define NOTIFY_VOLUP    0x15
#define NOTIFY_BRN_MIN  0x20
#define NOTIFY_BRN_MAX  0x2f
#define VOLUME_STEP     10

typedef ULONG (APIENTRY *MCISENDSTRING)(PSZ, PSZ, USHORT, HWND, USHORT);

static MCISENDSTRING SendString;
static ACPI_HANDLE Atkd;
static FILE *Log;
static int Muted;

static void note(const char *format, ...)
{
    va_list args;
    time_t now;
    char stamp[32];

    if (!Log) return;
    time(&now);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(Log, "%s ", stamp);
    va_start(args, format);
    vfprintf(Log, format, args);
    va_end(args);
    fputc('\n', Log);
    fflush(Log);
}

/* Commande MCI ; la réponse éventuelle va dans reply. */
static ULONG mci(const char *command, char *reply, USHORT size)
{
    char empty[4];

    if (!SendString) return 1;
    if (!reply) { reply = empty; size = sizeof(empty); }
    reply[0] = '\0';
    return SendString((PSZ)command, (PSZ)reply, size, NULLHANDLE, 0);
}

static int master_volume(void)
{
    char reply[32];

    if (mci("masteraudio query volume wait", reply, sizeof(reply))) return 50;
    return atoi(reply);
}

static void change_volume(int delta)
{
    char command[64];
    int volume = master_volume() + delta;

    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (Muted) {
        mci("masteraudio on wait", NULL, 0);
        Muted = 0;
    }
    sprintf(command, "masteraudio volume %d wait", volume);
    mci(command, NULL, 0);
}

static void toggle_mute(void)
{
    Muted = !Muted;
    mci(Muted ? "masteraudio off wait" : "masteraudio on wait", NULL, 0);
}

/* Méthode de \_SB.ATKD prenant un entier. */
static ACPI_STATUS call_int(char *method, UINT32 value)
{
    ACPI_OBJECT argument;
    ACPI_OBJECT_LIST arguments;

    argument.Type = ACPI_TYPE_INTEGER;
    argument.Integer.Value = value;
    arguments.Count = 1;
    arguments.Pointer = &argument;
    return AcpiTkEvaluateObject(Atkd, method, &arguments, NULL);
}

/* Fn+F9 : la liste des fenêtres, comme Ctrl+Échap. */
static void show_window_list(void)
{
    HAB hab = WinInitialize(0);
    HMQ hmq = WinCreateMsgQueue(hab, 0);

    WinPostMsg(WinQueryWindow(HWND_DESKTOP, QW_BOTTOM), WM_SYSCOMMAND,
               MPFROMSHORT(SC_TASKMANAGER), MPFROM2SHORT(CMDSRC_ACCELERATOR, FALSE));
    if (hmq) WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
}

/* Cherche le périphérique dont le _HID est ASUS010. */
static ACPI_STATUS find_eee(ACPI_HANDLE object, UINT32 level, void *context, void **found)
{
    ACPI_DEVICE_INFO *info = NULL;

    (void)level; (void)context;
    if (AcpiTkGetObjectInfoAlloc3(object, &info) == AE_OK && info) {
        if ((info->Valid & ACPI_VALID_HID) && info->HardwareId.String
            && strcmp(info->HardwareId.String, EEE_HID) == 0) {
            *found = object;
            AcpiTkOsFree(info);
            return AE_CTRL_TERMINATE;
        }
        AcpiTkOsFree(info);
    }
    return AE_OK;
}

int main(int argc, char *argv[])
{
    ACPI_API_HANDLE api;
    ACPI_HANDLE handle;
    UINT32 event;
    HMODULE mdm;
    char failure[CCHMAXPATH];
    char *log_dir = getenv("LOGFILES");
    char path[CCHMAXPATH];
    void *found = NULL;
    int quiet = argc > 1 && strcmp(argv[1], "-q") == 0;

    sprintf(path, "%s\\EEEKEYS.LOG", log_dir ? log_dir : "C:\\VAR\\LOG");
    Log = fopen(path, "a");

    if (AcpiTkValidateVersion(ACPI_TK_VERSION_MAJOR, ACPI_TK_VERSION_MINOR)) {
        note("version du pilote ACPI incompatible avec le kit");
        return 1;
    }
    if (AcpiTkStartApi(&api)) {
        note("pilote ACPI injoignable (ACPI.PSD absent ?)");
        return 1;
    }
    if (AcpiTkGetHandle(NULL, "\\_SB.ATKD", &Atkd) != AE_OK) {
        AcpiTkWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, find_eee, NULL, &found);
        Atkd = found;
    }
    if (!Atkd) {
        note("aucun périphérique %s : ce n'est pas un Eee PC ?", EEE_HID);
        if (!quiet) printf("eeekeys : aucun peripherique %s.\n", EEE_HID);
        return 1;
    }
    /* Active les touches ; 0 : le BIOS garde le Wi-Fi et l'écran externe. */
    note("démarrage, INIT -> %s", AcpiTkStatusToStr(call_int("INIT", 0)));

    if (DosLoadModule(failure, sizeof(failure), "MDM", &mdm) == NO_ERROR)
        DosQueryProcAddr(mdm, 0, "mciSendString", (PFN *)&SendString);
    if (!SendString) note("MDM.DLL absent : pas de réglage du volume");

    for (;;) {
        handle = 0;
        event = 0;
        if (AcpiTkWaitNotify(&api, &handle, &event, ACPI_ALL_NOTIFY, (UINT32)-1) != AE_OK) {
            DosSleep(1000);
            continue;
        }
        if (handle != Atkd) continue;
        switch (event) {
        case NOTIFY_MUTE:    toggle_mute(); break;
        case NOTIFY_VOLDOWN: change_volume(-VOLUME_STEP); break;
        case NOTIFY_VOLUP:   change_volume(VOLUME_STEP); break;
        case NOTIFY_TASKS:   show_window_list(); break;
        default:
            if (event >= NOTIFY_BRN_MIN && event <= NOTIFY_BRN_MAX)
                call_int("PBLS", event - NOTIFY_BRN_MIN);
            else
                note("notification 0x%02lX non traitée", (unsigned long)event);
        }
    }
}
