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
- show/hide branches

Issues:
- (Partial) fix spouse separation when multiple spouses have descendants (connecting line collision)
- zoom properly about mouse position, and keep root person in view when selecting

Building Fambiz:
- build from solution (.sln) file in VC 2013 or later
- only need fambiz.exe
