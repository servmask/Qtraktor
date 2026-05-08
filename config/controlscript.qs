// Installer-level controller script.
//
// Qt IFW's Installation Folder page rejects target directories that
// already contain an IFW install ("The directory you selected already
// exists and contains an installation. Choose a different target for
// installation."). That breaks the WinSparkle auto-update flow, which
// downloads a fresh installer .exe and runs it over the existing copy.
//
// Workaround: at the welcome page, detect a previous install at the
// default location and recursively delete it via cmd.exe before the
// wizard ever shows the directory page. By the time the user clicks
// Next, the previous install is gone and the directory check passes.
//
// We do NOT run the previous install's maintenancetool.exe here, even
// though it would be the formally-correct uninstall path. Qt IFW
// enforces a process-singleton lock across all installer-family
// binaries: spawning maintenancetool.exe from inside this running
// installer fails with "Another maintenancetool instance is already
// running" (verified live during the windows-auto-update test). cmd.exe
// runs no Qt IFW machinery so it bypasses the lock entirely. Old
// registry entries (Uninstall key, file associations, shortcuts) are
// re-written by installscript.qs on the new install, so the end state
// is clean.
//
// First-time installs are unaffected: maintenancetool.exe doesn't exist
// yet, so the fileExists() check is false and we fall through.

function Controller()
{
}

// Skip the Component Selection wizard page. Traktor ships as a single
// IFW component (com.servmask.traktor) which is always installed, so the
// page would only show one always-checked checkbox with no user decision
// to make. Auto-advancing keeps the wizard tight.
Controller.prototype.ComponentSelectionPageCallback = function()
{
    gui.clickButton(buttons.NextButton);
};

Controller.prototype.IntroductionPageCallback = function()
{
    // ApplicationsDir resolves to %ProgramFiles% on Windows admin installs;
    // the per-user fallback resolves elsewhere. Match the same default the
    // <TargetDir> in config.xml uses.
    var defaultDir = installer.value("ApplicationsDir") + "/Traktor";
    var maintTool = defaultDir + "/maintenancetool.exe";
    if (installer.fileExists(maintTool)) {
        // C:\Program Files\Traktor requires admin to delete. WinSparkle
        // launches the installer as the current user, so we must elevate
        // before rmdir or it fails silently and the directory check on
        // the next page fires. gainAdminRights() triggers a UAC prompt
        // if the process is not already elevated; this is the same UAC
        // prompt Qt IFW would have shown later anyway when copying files
        // into Program Files, so it adds no extra friction.
        installer.gainAdminRights();
        // cmd.exe wants backslashes for rmdir; convert from the
        // forward-slash form Qt IFW returns.
        var defaultDirNative = defaultDir.split("/").join("\\");
        // /s recurse, /q no confirmation. installer.execute() blocks
        // until cmd.exe exits.
        installer.execute("cmd.exe", ["/c", "rmdir", "/s", "/q", defaultDirNative]);
    }
};
