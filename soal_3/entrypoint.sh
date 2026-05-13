#!/bin/bash


sed -i '/imklog/s/^/#/' /etc/rsyslog.conf
rsyslogd
sleep 2

groupadd readonly
groupadd staff

useradd -M -s /bin/false -g readonly member
(echo "member123"; echo "member123") | smbpasswd -a -s member

useradd -M -s /bin/false -g staff contributor
(echo "contrib456"; echo "contrib456") | smbpasswd -a -s contributor

useradd -M -s /bin/false -g staff librarian
(echo "lib789"; echo "lib789") | smbpasswd -a -s librarian

mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs
chmod 755 /libraryit

chown -R root:staff /libraryit/ebooks /libraryit/papers
chmod 775 /libraryit/ebooks /libraryit/papers

chown -R root:staff /libraryit/sourcecode
chmod 750 /libraryit/sourcecode

chown -R librarian:staff /libraryit/docs
chmod 755 /libraryit/docs


touch /logs/libraryit.log
chmod 666 /logs/libraryit.log
touch /var/log/syslog
chmod 666 /var/log/syslog


tail -F /var/log/syslog | awk '
/smbd_audit:/ {
    split($0, a, "smbd_audit: ");
    split(a[2], b, "|");
    user = b[1]; share = b[2]; op = b[3]; status = b[4]; file = b[5];

    level = "INFO";
    action = "";

    if (op == "connect") {
        if (status == "fail") { 
            action = "DENIED"; 
            level = "WARNING"; 
            file = share;
            if (file == "sourcecode") file = "SourceCode";
        } else { 
            action = "CONNECT"; 
            file = share; 
            if (file == "sourcecode") file = "SourceCode";
        }
    } else if (op == "open" && file != ".") {
        action = "WRITE";
    }

    if (action != "") {
        "date \"+%Y-%m-%d %H:%M:%S\"" | getline ts;
        close("date \"+%Y-%m-%d %H:%M:%S\"");
        printf "[%s] [%s] [%s] [%s] [%s]\n", ts, level, user, action, file >> "/logs/libraryit.log";
        fflush("/logs/libraryit.log");
    }
}
' &


echo "LibraryIT Server is up and running!"
exec smbd --foreground --no-process-group