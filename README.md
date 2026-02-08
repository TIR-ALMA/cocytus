# Cocytus
This is an extremely stealthy and destructively malicious software, one of the projects from "Ponds of Hell." Cocytus is a UEFI rootkit + bootkit + payload injector. It is built upon the source code of the BlackLotus malware. Cocytus's goal is to load into the UEFI, for example, from a Trojan, launch before Windows, stop the cooling fans, maximally stress and overheat the CPU, GPU, RAM, and storage drive, and simply completely physically destroy a device running Windows. It works on Windows 7–11 and newer versions of Windows and successfully bypasses many antivirus programs.

It differs from the ordinary BlackLotus in that Cocytus cannot be controlled from a C2 server; it is autonomous, which allows it to be even more stealthy. And maximum stress on the device has been added.
