Option Explicit

' This file is not a part of the public interface and can be removed without
' notice in future versions.
'
' This script adds the TARGETDIR property to an .msi file
' Parameters: 1 - Filename
Dim MSI_FILE
MSI_FILE = WScript.Arguments(0)

Dim installer, database, view

Set installer = CreateObject("WindowsInstaller.Installer")
Set database = installer.OpenDatabase(MSI_FILE, 1)

Set view = database.OpenView("INSERT INTO Property (Property, Value) VALUES ('TARGETDIR', 'C:\Program')")
view.Execute

'Set view = database.OpenView("DELETE FROM Directory WHERE Directory='TARGETDIR'")
'view.Execute
'Set view = database.OpenView("UPDATE Directory SET DefaultDir='INSTALLLOCATION' WHERE Directory='TARGETDIR'")
'view.Execute

database.Commit

Set database = Nothing
Set installer = Nothing
Set view = Nothing

