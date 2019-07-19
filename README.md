# Family Business (fambiz)
Family tree maintenance and chart printing.

Since the free family tree programs are weak in chart printing or have other limitations (and I don't like subscribing to the non-free ones) here is an attempt to fill in the gaps.

Capabilities:
- runs on Windows 7-10
- read and write GEDCOM files as used by many other genealogy programs
- visualise and print family tree charts (descendant, ancestor or combined)

Non-goals:
- research and linkage to the web
- pretty printing

Todos:
- save view (zoom/scroll/root) with file
- edit and enter people and family relationships
- disconnect person or branch and reattach elsewhere
- move branch (not just a single person)
- show/hide branches

Issues:
- families/spousal relationships need to be oldest on left (add to tail when reading)
- possible GDI resource leak
- fix spouse separation when multiple spouses have descendants (connecting line collision)

Building Fambiz:
- build from solution (.sln) file in VC 2013 or later
- only need fambiz.exe
