MIDI MONITOR DLL FOR POWERTRACKS PRO AUDIO              (readme.txt)
by Eric Rangell
------------------------------------

INSTALLATION INSTRUCTIONS:

1. Determine the directory where PowerTracks Pro Audio resides.  It is
   usuallyin the C:\PT directory.

2. If you are upgrading from a previous version, you should save a backup
   copy of your existing $MIDMON.INI file so you can compare any changes
   you made to the new version of this file and edit it as necessary.

   Copy the following two files to the Powertracks directory:

        $MIDMO22.DLL    = The MIDI Monitor DLL (version 2.02)
        $MIDMON.INI     = Instrument Color Mappings and configuration info

3. Start up PowerTracks Pro, and go to the MIXER window.

4. Click the button labelled: DLL or EXE, and select the file: MIDMO22.DLL

   The INI file will be read from the current directory.
   If you get a message the the INI file could not be found, follow
   the displayed instructions to load the INI file manually using the
   M key, and then press the X key twice to redraw the display.

5. Load your favorite song or jukebox file into Powertracks.
   Note: You can also load a song from the Midi Monitor using the 'N' key,
   as described below.  You also have the option of optimizing the column
   display for the current song before you play it.

6. Start the song playing, then press ALT TAB to switch to the
Midi Monitor window.  You will see 16 columns which represent
the 16 midi channels.  Notes will appear as colored bars where
the color represents the instrument or patch, the length
represents the velocity or volume, and the vertical position represents
the pitch (high notes are at the top).  Pitch bends will display
as bars below the column but above the channel number, and they
will grow and shrink depending on how far the notes are bent.

NOTE: Only MIDI tracks will be displayed.  Audio tracks will not display.

Controller changes will display as lines in the boxes on the right.
Each box represents one controller (volume, modulation, panpot,
chorus, reverb, expression), and within each box are up to 16 bars,
one for each midi channel where 1 is at the top and 16 is at the
bottom.  Use the tick marks on the left edge of each box to figure
out which channel you are looking at (The longest tick marks are
channels 8 and 16).  Pedals will appear as boxes on the bottom right.

Starting with 1.07, history trails will appear when note off events occur.
They will disappear shortly afterwards, depending on a time parameter
for each channel that is set up in the INI file.

Starting with 1.09, you can configure the number of columns to be displayed
and which channels are displayed in each column.  You can run the
C:\MIDISTAT program to create an optimal configuration for any .MID file,
based on note ranges, pan values, and channel usage.  That program creates
an output file: C:\MIDISTAT.OUT which contains the statements that can
either be added to your C:\PT\$MIDMON.INI file or dynamically loaded into
the Midi Monitor using the 'X' key described below.  You may decide to
edit the MIDISTAT.OUT file first, (for example if you want to remove the
display of a drum part), and you can copy it to another filename
(I suggest the name of the MIDI file followed by a .MON extension) so
that you can reuse it at a future time.

Starting with 2.00, you can optimize the display from within PowerTracks pro!
Just press the letter 'O' key after loading a song, and click OK.  The DLL
will read all events of the currently loaded song, and create the
C:\MIDISTAT.OUT file using that data.  When it is done, you can press
the 'X' key twice to update the display.  Also, you can mute tracks in
the PowerTracks Track window and they will be excluded from the optimization.
This is useful if you don't want to monitor a track, such as a drum part.

The other new feature is the ability to change the tempo of a song from
the midi monitor while the song is playing!  Use the 'U' and 'D' keys to
make the tempo go UP or DOWN by 10%.  These keys affect all events in the
song's tempo map, so there should be smooth transitions between different
sections when the tempo is adjusted.

Starting with 2.01, you can load .MID, .SEQ, .KAR, or .DP files directly
from the monitor using the 'N' key.  You can also use the 'M' key to load
a .MON file containing configuration parameters stored in the same format
as the INI file described later.  One little trick you can also do is
reload the settings from your $MIDMON.INI file by pressing 'M' and typing
the full path name (usually C:\PT\$MIDMON.INI).  You then press the X key
twice to redraw the monitor.  This works because all the 'M' key does
is copy the file you pick to C:\MIDISTAT.OUT.  Other new keystrokes
described below are: T, I, V.

Starting with 2.02, you can resize the display window (either by dragging
the borders using the mouse or using the "Size" option of the control menu),
and then press the X key twice to redraw the column display to fit inside
the window.


7. Use the following keys/mouse clicks to control the program:

	Left Mouse button = Toggle between Sharp/Flat display
	Right Mouse button = Toggle between Graphic/Text Note display

        N       = Load a new .MID, .SEQ, .KAR, or .DP file into PowerTracks
        M       = Load a .MON file of monitor configuration parameters
        O       = Optimize the display for the current song
	P       = Play the song from the current position
	S       = Stop the song from playing
	space   = Toggle - if playing stops it, if not playing starts it
	W       = Rewind the song to the beginning

	H       = Switch to SHARP Text Note display
	L       = Switch to FLAT Text Note display
	B       = Switch to graphic BAR display

        T       = Toggle between Multi-Color and Single Color Text modes
        I       = Make the mouse pointer invisible
        V       = Make the mouse pointer visible

        U       = Multiply all tempo events in the song by 1.10
        D       = Divide all tempo events in the song by 1.10

	G       = Get the FILENAME of the song that is playing
        X       = Erase and redraw the main display using C:\MIDISTAT.OUT 
                  (you may have to press it twice for it to redraw correctly)
	R       = Redraw the controller display (if it got blanked out)
	Z       = Erase the controller display and forget previous values


	The controller display will get blanked out if you switch to
	another program and then return to the Midi Monitor.  Just press
	R to redraw it with whatever values were last saved for each
	controller on each channel.  If you press Z then these values
	will be zeroed out so only new controller data will be displayed.

OTHER FILES:
        MIDMO22.C       = Visual C++ 1.52 source code
        MIDMO22.DEF     = Module definition file for the DLL
        MIDMO22.MAK     = Make file
        MIDMO22.RC      = Resource file (needed for Open File dialogs)
        RESOURCE.H      = Resource header
        MIDIMON.TXT     = Old Specifications

	If you want to compile the program, load the Make file as a project
        and rebuild the DLL.  You can either move all the files to a
        C:\PROJECTS\MIDMON22 directory, or change all references to that
        directory in the make file.


HOW TO CHANGE THE COLORS:
=========================
Starting with version 1.03, you can now change the colors of the
instruments by editing the $MIDMON.INI file!

Starting with version 1.07, you can now determine how long the history
trails will stay on screen for each channel, and also choose to apply
event transforms to convert aftertouch/channel pressure to volume.

Starting with version 2.01, you can change the color used by the
Text Note display in Single Color mode by adding a line to the
[Patch Colors] section.

NOTE: The INI file must be stored in the C:\PT directory because that
is what is hardcoded in the program.  If the INI file is not found, the
program uses hardcoded default values for all parameters.

The INI file has 5 sections:

[Color Names]   = where you define new colors
[Patch Names]   = where you define your instrument's bank and patch names
[Patch Colors]  = where you assign colors to patches
[Transforms]    = to choose preprogrammed event transforms
[Faders]        = to set time values for history trails

The file is read sequentially when the DLL is launched from the PowerTracks
Mixer window.  This means you can define a patch name to represent a
range of instrument numbers, assign a default color to that whole range,
and then override specific instruments in that range to be a different color.

For example, in the INI file, the name "ensemble bank" is assigned to
instrument numbers 49-56 and given a default color of dark pink.  However,
I wanted a different color for two of the instruments in that range
(choir aahs and voice oohs) so I defined the following names:

	voice oohs = 54
	choir aahs = 53

and then changed the color of those two instruments with the statements:

	voice oohs = light blue
	choir aahs = orange

So, first you define any new colors you want to use in the [Color Names]
section by adding statements of the form:

	<color name> = <red> , <green> , <blue>

	where red, green, and blue are integers from 0-255
	that represent the proportions of each primary color.

	You can define up to 200 color names.
	Each color name must be less than 40 characters, and the
	equal sign must appear before column 40.

Then you define your patch names in the [Patch Names] section using
one of the following 2 formats:

	<patch name> = <instrument #> - <instrument #>

	or

	<patch name> = <instrument #>

	where instrument # is a value from 1 to 128.

	You can define up to 200 patch names.
	Each patch name must be less than 40 characters, and the
	equal sign must appear before column 40.


Finally, you define your [Patch Colors] as follows:

	<patch name> = <color name>

	where patch name was defined in the Patch Names section
	and color name was defined in the Color Names section.

When done, copy the new INI file to your Powertracks directory (it must
be named $MIDMON.INI.  If it is not found, default colors are used.)
and start up the DLL.  (If you already had the DLL running, you
need to click the DLL button on the mixer window and reselect
MIDMO21.DLL in order for the new colors to take effect.)

The song must have Program Change events in order to display the
correct colors.  You can assign program changes to tracks within
Powertracks, and they will be sent when the song starts playing.

New for version 2.01:
In the [Patch Colors] section, you can add a line like the following:

        text = yellow

which will set the color to be used when the text note display is in
single color mode.  (Use the 'T' key to toggle between multi-color and
single color modes.)

Also note that you can now save your configuration settings in a file
with a .MON extension (instead of always editing $MIDMON.INI), and use
the 'M' key to load them into the Midi Monitor at any time.


IMPLEMENTATION OF MIDI EVENT TRANSFORMS IN POWERTRACKS MIDI MONITOR DLL
=======================================================================
The PowerTracks MIDI Monitor DLL works by reading a circular buffer of
MIDI output data produced by Powertracks Pro.  Powertracks merges data  
from the midi file in memory with data being received from MIDI IN.  
The data from MIDI IN will be sent to MIDI OUT if the MIDI THRU mode of
Powertracks is enabled (using Options...Midi Thru).  However, the MIDI
THRU mode must be enabled in order for the DLL to receive messages
from Powertracks.

In previous versions, the DLL did not do any manipulation or output of 
the midi data it received.  In order to implement event transforms
it was modified to allow the user to select preprogrammed event
transforms to be applied to the midi data.  If the user desires this
behavior, it can be turned on by editing the INI file and adding
keywords to a section called [Transforms].  

The following keywords control the event transform behavior.

SoftThru=1                 needed to allow the DLL to send midi out messages
NoDisplay=0                needed to draw all events on midi monitor
			   (can be turned off with NoDisplay=1)
AfterTouchToVolume=1       to transform aftertouch events to volume 
ChannelPressureToVolume=1  to transform channel pressure events to volume

Note: the original aftertouch and channel pressure messages will still
be sent because Midi Thru mode is enabled.  However, the DLL will send the
volume change events immediately after the aftertouch/channel pressure
events.

The hexadecimal mappings for the event transforms are as follows:

Aftertouch: Ax yy zz    -->     Bx 07 zz

Channel Pressure: Dx yy -->     Bx 07 yy

Try playing your midi sax with this program running to test it out!
The breath controller should vary the length of the line in the volume box.


IMPLEMENTATION OF HISTORY TRAILS
================================
History Trails were implemented in version 1.07 thanks to a suggestion by
Stephen Malinowski.

History Trails are drawn when NOTE OFF events occur.  They remain on screen
for a predetermined time for each channel.  You can set up this time value
in the INI file, in the section called [Faders].

Each line of the [Faders] section indicates a channel number (1-16)
and the number of timer events for which to keep the history trail on
screen for that channel.  A timer event is defined in the program to
be 50 milliseconds, so a value of 5 timer events is about 1/4 second.
If a channel does not have a row in the INI file, a value of 5 is
assumed as a default.  If you set the value to 0 for a channel, then
no history trail will be drawn.  The maximum allowed value is 100.

For example:
[Faders]
1=5             gives a delay of about 1/4 second before trail erased
2=5
3=5
4=10            about 1/2 second
10=20           about 1 second
11=0            no trail will be produced


In version 1.08, the fade process was divided into 5 phases.  Each phase
occurs for 20% of the total fade time.  You can define the color and width
of the line to be drawn for each phase using the following sections
of the INI file:

[FaderPhaseColors] - defines the color to be used for each phase

Each line in this section consists of a phase number (1-5) followed by
an equal sign, followed by a color name defined in the [Color Names]
section.  For example:

1=light gray

indicates that the color light gray will be used for phase 1 of the fade.

[FaderPhaseWidths] - defines the width of the line to be drawn at each phase

Each line in this section consists of a phase number (1-5) followed by
an equal sign, followed by a line width (1-4).  For example:

1=2

indicates that phase 1 will be drawn with a line width of 2.

You'll need to experiment with different colors and widths until you
find one that appeals to you.  Remember that whenever you edit the
INI file, you have to unload the DLL (by picking a different one)
and then reload it from the Powertracks Mixer window.

CONFIGURATION OF COLUMN ORDER
=============================
Starting with 1.09, you can add statements to your C:\PT\$MIDMON.INI file
to describe the number of columns to display, which channels display in
each column, and the lowest and highest notes used in your song.  This will
allow the display to be scaled - you'll get bigger bars by using smaller
note ranges or fewer columns.

To use this feature, add the following section to your $MIDMON.INI file:
[ColumnChannels]

In this section, the following variables can be set:

LowNote=21   (the MIDI note number of the lowest note that will be displayed)
HighNote=108 (the MIDI note number of the highest note that will be displayed)
NumCols=2   (the number of columns to be displayed)
1=1          (each of these lines contains: Column Number = Channel Number)
2=10         (ex: this will display MIDI Channel 10 data in column #2)   

If you do not assign a channel to each column that will be displayed
then that column will be blank upon playback.  You also should not
assign the same channel to more than 1 column - only the rightmost
column will be used and the rest will be blank.

For version 2.00, you don't have to shell to dos to optimize the display.
Just press the letter 'O' key after loading a song, then press 'X' twice.

Enjoy your new toy!!!
Please report any bugs and enhancement suggestions to erang@compuserve.com
and also let me know if you find any practical applications for this program.

Visit Crazy Eric's website at http://ourworld.compuserve.com/homepages/erang

Revision History
================
1.00 - Worked in Powertracks 3.5 but GPF'd in Powertracks 3.5a
       This was caused by a problem with the color assignment routine.
1.01 - Took out the color assignment routine to prove the cause of the GPF.
1.02 - Found a way to get the color assignment routine to work.
       Also, made the bars bigger by using the piano note range (21-108)
       instead of the full midi range (0-127), and made the color changes
       apply to the pitch bends as well as the notes.
1.03 - Allows user to change colors by editing INI file, and performance
       enhancement by using timer callback function instead of WM_TIMER
       and only repainting screen when midi events are detected.
       Now distributing documentation and source code to release the
       power to everyone.
1.04 - Controller displays, and combined text and graphic monitors
       into one program.
1.05 - Fixed bug where strings on Powertracks Tracks window were corrupted
       after the DLL loaded.  This was caused because the device context
       had been declared as static in the windowproc so it could be used
       in the wm_char processor.  It was changed to be a global variable.
1.06 - Attempt to add history trails, but GPF'd after many songs played.
1.07 - I think I got rid of the GPF.  Uses INI file to get fader values
       for display of history trails.  Also includes code for Event
       Transforms (previously published as TRANSFRM.ZIP)
1.08 - Enhanced history trails to allow different colors and lengths
       for each of the 5 phases of a fade.
1.09 - Added ability to configure number of columns displayed and the
       channel for each column.  Included an optimizer program to
       determine the best configuration for any .MID file.
2.00 - Added 'O' function to optimize display from within PowerTracks
       and 'U' and 'D' to do tempo changes while a song is playing.
2.01 - Improved text note display, and added functions to load midi
       and MON files, toggle text note display between single and
       multi-color, make mouse invisible or visible.  Also fixed
       optimization routine for forced channels.
2.02 - Finally figured out how to get the window to resize properly.
       Fixed problem where users who installed Powertracks in a directory
       other than C:\PT couldn't load the INI file, and improved error
       handling for this case.

