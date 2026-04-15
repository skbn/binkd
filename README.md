Binkd is a Fidonet mailer designed to operate via TCP/IP networks.

As a FTN-compatible internet daemon, it makes possible efficient
utilization of TCP/IP protocol suite as a transport layer in
FTN-based (Fido Technology Network) networks.

## Compiling

AmigaOS:

Copy "mkfls/amiga/Makefile" to root and make

Need ADE to compile project with ixemul library.

https://aminet.net/package/dev/gcc/ADE

It no longer requires ADE to be installed for execution, as it doesn't need /bin/sh to run scripts or external programs from binkd.conf. However, it does require ixemul.library and ixnet.library, either in the libs directory or in the same directory as the executable.

You can find the ADE ixemul.library and ixnet.library libraries in the aminet package or in the released versions, along with the program and utilities already compiled.

https://github.com/skbn/binkd/releases

5.Work:fido> version work:fido/ixnet.library 
ixnet.library 63.1

5.Work:fido> version work:fido/ixemul.library 
ixemul.library 63.1


I've attached six programs for your assistance:

[decompress] which decompresses incoming files in lha or zip format, if necessary.

[freq] which generates file requests in ASO mode and places the necessary files in outbound directory.
[freq_bso] same but BSO style

[process_tic] which processes tic files and places them in the filebox folder.
With --copypublic option, copy the file you receive to a directory named pub/ from PROGDIR
>> exec "path/process_tic --copypublic" *.tic *.TIC
>> exec "path/process_tic" *.tic *.TIC

[srifreq] copy of "misc/srifreq" but in c
Processes file requests arriving at binkd. The files must be in pub/ or use the environment variables:
SRIFREQ_PUBDIR Directory with public files (default: pub/)
SRIFREQ_LOG Log file (optional)

>> exec "path/srifreq *S" *.req

[nodelist] FidoNet nodelist compiler for binkd - AmigaOS version in c from "misc/nodelist.pl"
>> nodelist <nodelist_file> <domain> [<output_file>]

Also compileable on *nix >> "gcc -O2 -Wall -o nodelist nodelist.c"

BUGFIXES:
Option "-C" to reload config It remains unstable

non-UNIX:

1. Find in mkfls/ a subdirectory for your system/compiler, copy all files 
   to the root of the sources.
2. Run make (nmake, wmake or gmake, name of make's binary is rely with C
   compiler).

UNIXes:

1.) Clone the repo: 

`$ git clone https://github.com/pgul/binkd`

2.) Change into the new binkd source directory:

`$ cd binkd`

3.) Copy all files from mkfls/unix/ to the root of binkd sources:

`cp mkfls/unix/* .`

2.) Run configure and make:

`$ ./configure`
`$ make`

3.) When finished, the following instructions will be displayed offering various options for you:

```
 Binkd is successfully compiled.

 If you want to install Binkd files into /usr/local
     1. Run `make -n install' to be sure this makefile will
        do not something criminal during the installation;
     2. `su' to root;
     3. Run `make install' to install Binkd.
     4. Edit /usr/local/etc/binkd.conf-dist and RENAME it or
        MOVE it somewhere (so another `make install' will
        not overwrite it during your next Binkd upgrade)

 If you want to put the files into some other directory just
 run `configure --prefix=/another/path' and go to step 1.
```

## Installation

1. Edit sample binkd.cfg.
2. Run binkd.

## More info

**Echomail areas:**
* RU.BINKD (russian)
* BINKD (international)

**Web site:** http://www.corbina.net/~maloff/binkd/

**FTP:** ftp://happy.kiev.ua/pub/fidosoft/mailer/binkd/

**The mirrors:**
   * ftp://fido.thunderdome.us/pub/mirror/binkd/
   * ftp://cube.sut.ru/pub/mirror/binkd/
   * http://binkd.spb.ru

**Documentation:**

* [English manual for binkd 0.9.2](http://web.archive.org/web/20131010041927/http://www.doe.carleton.ca/~nsoveiko/fido/binkd/man/) © Nick Soveiko (<nsoveiko@doe.carleton.ca>)
* [Russian manual for binkd 0.9.9](http://binkd.grumbler.org/binkd-ug-ru.htm.win.ru) © Stas Degteff (`2:5080/102@fidonet`)
* [FAQ](http://binkd.grumbler.org/binkdfaq.shtml)

**Authors:** Dmitry Maloff <maloff@corbina.net> and others.

**Bug reporting:** <binkd-bugs@happy.kiev.ua>, also RU.BINKD or BINKD echoconferences.

**Binkd developers mailing list:** <binkd-dev@happy.kiev.ua> (send `subscribe binkd-dev` to <majordomo@happy.kiev.ua> for subscribe).
