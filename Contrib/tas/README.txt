Speedrun tools for openMSX
==========================

The omr2txt.py tool allows extracting the input event log of an openMSX replay (OMR file) to a plain text format. The txt2omr.py tool performs the opposite operation: it reads an input event log in text format and inserts it into an OMR file.

Together, these tools can be used to view and edit your event logs in a text editor. They can also be used to filter events and align them to frame boundaries. And you could use them if you want to write your own scripts to generate or optimize event logs.

Another use for these tools is to be able to generate a version of your speedrun in progress that can be stored in a version control system with human-readable diffs.

The tools require Python version 3.x.


Limitations
-----------

Currently omr2txt.py will only convert key presses listed in its "inputMap" dictionary. If your game uses keys other than the cursors and space, add the name and the keyboard matrix row/column in this dictionary near the top of the script.

I wrote these tools when working on my King's Valley TAS, so I could replace the recording of one pyramid's solution without having to re-record all pyramids that come after it. While I did implement some rudimentary error handling, don't expect them to be robust to bad input.


Format
------

Empty lines and lines starting with "#" are ignored.

Lines starting with "=" are processing instructions:

= base wsx-kv1.omr
= base wsx-kv1.xml
Specifies the recording to use as a base when converting back to OMR. The initial snapshot of this recording sets up the MSX machine and the game. Both ".omr" (OMR file, which is gzip-ed XML) and ".xml" (uncompressed XML from OMR file) file name extensions are supported.

= out kings_valley_mth_v0.omr
Pathname of the output file that txt2omr.py will write.

= scale 57346560
Number of master clock ticks per time unit (frame). All following time intervals in the event log will be multiplied by this number.

= input r key 8 7
Defines an input name "r" as a key press of keyboard matrix row 8, column 7. Currently key presses are the only supported types of events.

= include pyramid01.txt
Includes another text file. Can be used to store the input events for every level in a separate file, for example.

All other lines are event log entries. Log entries are whitespace-separated lists, where the first element is a time interval length and the following elements are the input events that are active during that interval.

For example:
   2  r
   1  r s
  24
This will press the "r" input, as defined earlier with a "=input" processing instruction, for two frames. Then it will hold "r" pressed while also pressing the "s" input for one frame. Finally it will release both inputs and wait for 24 frames.


License
-------

Copyright (c) 2015  Maarten ter Huurne  <maarten@treewalker.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
