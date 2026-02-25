function Component() {
    // Request admin rights since we install to Program Files
    installer.gainAdminRights();
}

Component.prototype.createOperations = function() {
    component.createOperations();

    if (systemInfo.productType === "windows") {
        // Normalize to backslashes so the registry path is valid on Windows
        var targetDir = installer.value("TargetDir").split("/").join("\\");
        var exePath = targetDir + "\\Traktor.exe";

        // Start Menu shortcut
        component.addOperation(
            "CreateShortcut",
            exePath,
            "@StartMenuDir@/Traktor.lnk"
        );

        // Desktop shortcut
        component.addOperation(
            "CreateShortcut",
            exePath,
            "@DesktopDir@/Traktor.lnk"
        );

        // Register .wpress file association
        // Command must quote the exe path (handles spaces in Program Files)
        var iconPath = targetDir + "\\file.ico";
        component.addOperation(
            "RegisterFileType",
            "wpress",
            "\"" + exePath + "\" \"%1\"",
            "WPRESS Backup File",
            "application/x-wpress",
            iconPath
        );
    }
};
