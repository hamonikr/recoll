# The MIT License (MIT)
# Copyright (c) 2013 Giles F. Hall
# https://github.com/vishnubob/python-midi/blob/master/LICENSE
# Modifications: Copyright (c) 2012-2018 J.F. Dockes
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation files
# (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 
from __future__ import print_function

import sys
from struct import unpack, pack
import six

def debug(s):
    print("%s"%s, file=sys.stderr)

PY3 = sys.version > '3'

if PY3:
    def next_byte_as_int(data):
        return next(data)
    def next_byte_as_char(data):
        return bytes([next(data)])
else:
    def next_byte_as_int(data):
        return ord(data.next())
    def next_byte_as_char(data):
        return next(data)

##
## Constants
##

OCTAVE_MAX_VALUE = 12
OCTAVE_VALUES = list(range( OCTAVE_MAX_VALUE ))

NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
WHITE_KEYS = [0, 2, 4, 5, 7, 9, 11]
BLACK_KEYS = [1, 3, 6, 8, 10]
NOTE_PER_OCTAVE = len( NOTE_NAMES )
NOTE_VALUES = list(range( OCTAVE_MAX_VALUE * NOTE_PER_OCTAVE ))
NOTE_NAME_MAP_FLAT = {}
NOTE_VALUE_MAP_FLAT = []
NOTE_NAME_MAP_SHARP = {}
NOTE_VALUE_MAP_SHARP = []

for value in list(range( 128 )):
    noteidx = value % NOTE_PER_OCTAVE
    octidx = value / OCTAVE_MAX_VALUE
    name = NOTE_NAMES[noteidx]
    if len( name ) == 2:
        # sharp note
        flat = NOTE_NAMES[noteidx+1] + 'b'
        NOTE_NAME_MAP_FLAT['%s-%d' % (flat, octidx)] = value
        NOTE_NAME_MAP_SHARP['%s-%d' % (name, octidx)] = value
        NOTE_VALUE_MAP_FLAT.append( '%s-%d' % (flat, octidx) )
        NOTE_VALUE_MAP_SHARP.append( '%s-%d' % (name, octidx) )
        globals()['%s_%d' % (name[0] + 's', octidx)] = value
        globals()['%s_%d' % (flat, octidx)] = value
    else:
        NOTE_NAME_MAP_FLAT['%s-%d' % (name, octidx)] = value
        NOTE_NAME_MAP_SHARP['%s-%d' % (name, octidx)] = value
        NOTE_VALUE_MAP_FLAT.append( '%s-%d' % (name, octidx) )
        NOTE_VALUE_MAP_SHARP.append( '%s-%d' % (name, octidx) )
        globals()['%s_%d' % (name, octidx)] = value

BEATNAMES = ['whole', 'half', 'quarter', 'eighth', 'sixteenth', 'thiry-second', 'sixty-fourth']
BEATVALUES = [4, 2, 1, .5, .25, .125, .0625]
WHOLE = 0
HALF = 1
QUARTER = 2
EIGHTH = 3
SIXTEENTH = 4
THIRTYSECOND = 5
SIXTYFOURTH = 6

DEFAULT_MIDI_HEADER_SIZE = 14


"""
EventMIDI : Concrete class used to describe MIDI Events.
Inherits from Event.
"""

class EventMeta(type):
    def __init__(cls, name, bases, dict):
        if name not in ['Event', 'MetaEvent', 'NoteEvent']:
            EventFactory.register_event(cls, bases)

@six.add_metaclass(EventMeta)
class Event(object):
    length = 0
    name = "Generic MIDI Event"
    statusmsg = 0x0

    class __metaclass__(type):
        def __init__(cls, name, bases, dict):
            if name not in ['Event', 'MetaEvent', 'NoteEvent']:
                EventFactory.register_event(cls, bases)

    def __init__(self):
        """ event type derived from __class__ """
        self.type       = self.__class__.__name__
        """ midi channel """
        self.channel    = 0
        """ midi tick """
        self.tick       = 0
        """ delay in ms """
        self.msdelay    = 0
        """ data after statusmsg """
        self.data       = b''
        """ track number """
        self.track      = 0
        """ sort order """
        self.order      = None

    def is_event(cls, statusmsg):
        return (cls.statusmsg == (statusmsg & 0xF0))
    is_event = classmethod(is_event)

    def __str__(self):
        return "%s @%d %dms C%d T%d" % (self.name, 
                            self.tick,
                            self.msdelay,
                            self.channel,
                            self.track)

    def __cmp__(self, other):
        if self.tick < other.tick: return -1
        elif self.tick > other.tick: return 1
        return 0
    def __lt__(self, other):
        return self.tick < other.tick
    def __eq__(self, other):
        return self.tick == other.tick

    def adjust_msdelay(self, tempo):
        rtick = self.tick - tempo.tick
        self.msdelay = int((rtick * tempo.mpt) + tempo.msdelay)
     
    def decode(self, tick, statusmsg, track, runningstatus=b''):
        assert(self.is_event(statusmsg))
        self.tick = tick
        self.channel = statusmsg & 0x0F
        self.data = b''
        if runningstatus:
            self.data += runningstatus
        remainder = self.length - len(self.data)
        if remainder:
            self.data += bytes.join(b'', [next_byte_as_char(track)
                                         for x in range(remainder)])
        self.decode_data()

    def decode_data(self):
        pass

    
"""
MetaEvent is a special subclass of Event that is not meant to
be used as a concrete class.  It defines a subset of Events known
as the Meta  events.
"""
    
class MetaEvent(Event):
    statusmsg = 0xFF
    metacommand = 0x0
    name = 'Meta Event'

    def is_event(cls, statusmsg):
        return (cls.statusmsg == statusmsg)
    is_event = classmethod(is_event)

    def is_meta_event(cls, metacmd):
        return (cls.metacommand == metacmd)
    is_meta_event = classmethod(is_meta_event)

    def decode(self, tick, command, track):
        assert(self.is_meta_event(command))
        self.tick = tick
        self.channel = 0
        if not hasattr(self, 'order'):
            self.order = None
        len = read_varlen(track)
        self.data = bytes.join(b'', [next_byte_as_char(track)
                                     for x in range(len)])
        self.decode_data()


"""
EventFactory is a singleton that you should not instantiate.  It is
a helper class that assists you in building MIDI event objects.
"""

class EventFactory(object):
    EventRegistry = []
    MetaEventRegistry = []
    
    def __init__(self):
        self.RunningStatus = None
        self.RunningTick = 0

    def register_event(cls, event, bases):
        if MetaEvent in bases:
            cls.MetaEventRegistry.append(event)
        elif (Event in bases) or (NoteEvent in bases):
            cls.EventRegistry.append(event)
        else:
            raise ValueError("Unknown bases class in event type: "+event.name)
    register_event = classmethod(register_event)

    def parse_midi_event(self, track):
        # first datum is varlen representing delta-time
        tick = read_varlen(track)
        self.RunningTick += tick
        # next byte is status message
        stsmsg = next_byte_as_int(track)
        # is the event a MetaEvent?
        if MetaEvent.is_event(stsmsg):
            # yes, figure out which one
            cmd = next_byte_as_int(track)
            for etype in self.MetaEventRegistry:
                if etype.is_meta_event(cmd):
                    evi = etype()
                    evi.decode(self.RunningTick, cmd, track)
                    return evi
            else:
                raise Warning("Unknown Meta MIDI Event: " + repr(cmd))
        # not a Meta MIDI event, must be a general message
        else:
            for etype in self.EventRegistry:
                if etype.is_event(stsmsg):
                    self.RunningStatus = (stsmsg, etype)
                    evi = etype()
                    evi.decode(self.RunningTick, stsmsg, track)
                    return evi
            else:
                if self.RunningStatus:
                    cached_stsmsg, etype = self.RunningStatus
                    evi = etype()
                    evi.decode(self.RunningTick, 
                            cached_stsmsg, track, bytes([stsmsg]))
                    return evi
                else:
                    raise Warning("Unknown MIDI Event: " + repr(stsmsg))

class NoteEvent(Event):
    length = 2
    fields = ['pitch', 'velocity']

    def __str__(self):
        return "%s [ %s(%s) %d ]" % \
                            (super(NoteEvent, self).__str__(),
                                NOTE_VALUE_MAP_SHARP[self.pitch],
                                self.pitch,
                                self.velocity)

    def decode_data(self):
        if PY3:
            self.pitch = self.data[0]
            self.velocity = self.data[1]
        else:
            self.pitch = ord(self.data[0])
            self.velocity = ord(self.data[1])


class NoteOnEvent(NoteEvent):
    statusmsg = 0x90
    name = 'Note On'

class NoteOffEvent(NoteEvent):
    statusmsg = 0x80
    name = 'Note Off'

class AfterTouchEvent(Event):
    statusmsg = 0xA0
    length = 2
    name = 'After Touch'

    def __str__(self):
        return "%s [ %s %s ]" % \
                            (super(AfterTouchEvent, self).__str__(),
                                hex(ord(self.data[0])),
                                hex(ord(self.data[1])))

class ControlChangeEvent(Event):
    statusmsg = 0xB0
    length = 2
    name = 'Control Change'
    
    def __str__(self):
        return "%s [ %s %s ]" % \
                            (super(ControlChangeEvent, self).__str__(),
                                hex(ord(self.data[0])),
                                hex(ord(self.data[1])))

    def decode_data(self):
        if PY3:
            self.control = self.data[0]
            self.value = self.data[1]
        else:
            self.control = ord(self.data[0])
            self.value = ord(self.data[1])


class ProgramChangeEvent(Event):
    statusmsg = 0xC0
    length = 1
    name = 'Program Change'

    def __str__(self):
        return "%s [ %s ]" % \
                            (super(ProgramChangeEvent, self).__str__(),
                                hex(ord(self.data[0])))

    def decode_data(self):
        if PY3:
            self.value = self.data[0]
        else:
            self.value = ord(self.data[0])


class ChannelAfterTouchEvent(Event):
    statusmsg = 0xD0
    length = 1
    name = 'Channel After Touch'

    def __str__(self):
        return "%s [ %s ]" % \
                            (super(ChannelAfterTouchEvent,self).__str__(),
                                hex(ord(self.data[0])))

class PitchWheelEvent(Event):
    statusmsg = 0xE0
    length = 2
    name = 'Pitch Wheel'

    def __str__(self):
        return "%s [ %s %s ]" % \
                            (super(PitchWheelEvent, self).__str__(),
                                hex(ord(self.data[0])),
                                hex(ord(self.data[1])))

    def decode_data(self):
        if PY3:
            first = self.data[0]
            second = self.data[1]
        else:
            first = ord(self.data[0]) 
            second = ord(self.data[1])
        self.value = ((second << 7) | first) - 0x2000


class SysExEvent(Event):
    statusmsg = 0xF0
    name = 'SysEx'

    def is_event(cls, statusmsg):
        return (cls.statusmsg == statusmsg)
    is_event = classmethod(is_event)

    def decode(self, tick, statusmsg, track):
        self.tick = tick
        self.channel = statusmsg & 0x0F
        len = read_varlen(track)
        self.data = bytes.join(b'', [next_byte_as_char(track)
                                     for x in range(len)])

class SequenceNumberMetaEvent(MetaEvent):
    name = 'Sequence Number'
    metacommand = 0x00

class TextMetaEvent(MetaEvent):
    name = 'Text'
    metacommand = 0x01

    def __str__(self):
        return "%s [ %s ]" % \
                            (super(TextMetaEvent, self).__str__(),
                            self.data)

class CopyrightMetaEvent(MetaEvent):
    name = 'Copyright Notice'
    metacommand = 0x02

class TrackNameEvent(MetaEvent):
    name = 'Track Name'
    metacommand = 0x03
    order = 3

    def __str__(self):
        return "%s [ %s ]" % \
                            (super(TrackNameEvent, self).__str__(),
                            self.data)

class InstrumentNameEvent(MetaEvent):
    name = 'Instrument Name'
    metacommand = 0x04
    order = 4

    def __str__(self):
        return "%s [ %s ]" % \
                            (super(InstrumentNameEvent, self).__str__(),
                            self.data)


class LryricsEvent(MetaEvent):
    name = 'Lyrics'
    metacommand = 0x05

    def __str__(self):
        return "%s [ %s ]" % \
                            (super(LryricsEvent, self).__str__(),
                            self.data)


class MarkerEvent(MetaEvent):
    name = 'Marker'
    metacommand = 0x06

class CuePointEvent(MetaEvent):
    name = 'Cue Point'
    metacommand = 0x07

class UnknownEvent(MetaEvent):
    name = 'whoknows?'
    metacommand = 0x09

class ChannelPrefixEvent(MetaEvent):
    name = 'Cue Point'
    metacommand = 0x20

class ChannelPrefixEvent(MetaEvent):
    name = 'Cue Point'
    metacommand = 0x20

class PortEvent(MetaEvent):
    fields = ['port']
    name = 'MIDI Port/Cable'
    metacommand = 0x21
    order = 5

    def __str__(self):
        return "%s [ port: %d ]" % \
                            (super(PortEvent, self).__str__(),
                            self.port)

    def decode_data(self):
        assert(len(self.data) == 1)
        if PY3:
            self.port = self.data[0]
        else:
            self.port = ord(self.data[0])

class TrackLoopEvent(MetaEvent):
    name = 'Track Loop'
    metacommand = 0x2E

class EndOfTrackEvent(MetaEvent):
    name = 'End of Track'
    metacommand = 0x2F
    order = 2

class SetTempoEvent(MetaEvent):
    fields = ['mpqn', 'tempo']
    name = 'Set Tempo'
    metacommand = 0x51
    order = 1

    def __str__(self):
        return "%s [ mpqn: %d tempo: %d ]" % \
                            (super(SetTempoEvent, self).__str__(),
                            self.mpqn, self.tempo)

    def __setattr__(self, item, value):
        if item == 'mpqn':
            self.__dict__['mpqn'] = value
            self.__dict__['tempo'] = float(6e7) / value 
        elif item == 'tempo':
            self.__dict__['tempo'] = value
            self.__dict__['mpqn'] = int(float(6e7) / value)
        else:
            self.__dict__[item] = value

    def decode_data(self):
        assert(len(self.data) == 3)
        if PY3:
            self.mpqn = (self.data[0] << 16) + (self.data[1] << 8) \
                        + self.data[2]
        else:
            self.mpqn = (ord(self.data[0]) << 16) + (ord(self.data[1]) << 8) \
                        + ord(self.data[2])

        self.tempo = float(6e7) / self.mpqn


class SmpteOffsetEvent(MetaEvent):
    name = 'SMPTE Offset'
    metacommand = 0x54

class TimeSignatureEvent(MetaEvent):
    fields = ['numerator', 'denominator', 'metronome', 'thirtyseconds']
    name = 'Time Signature'
    metacommand = 0x58
    order = 0

    def __str__(self):
        return "%s [ %d/%d  metro: %d  32nds: %d ]" % \
                            (super(TimeSignatureEvent, self).__str__(),
                                self.numerator, self.denominator,
                                self.metronome, self.thirtyseconds)
    if PY3:
        def decode_data(self):
            assert(len(self.data) == 4)
            self.numerator = self.data[0]
            # Weird: the denominator is two to the power of the data variable
            self.denominator = 2 ** self.data[1]
            self.metronome = self.data[2]
            self.thirtyseconds = self.data[3]
    else:
        def decode_data(self):
            assert(len(self.data) == 4)
            self.numerator = ord(self.data[0])
            # Weird: the denominator is two to the power of the data variable
            self.denominator = 2 ** ord(self.data[1])
            self.metronome = ord(self.data[2])
            self.thirtyseconds = ord(self.data[3])


class KeySignatureEvent(MetaEvent):
    name = 'Key Signature'
    metacommand = 0x59

class BeatMarkerEvent(MetaEvent):
    name = 'Beat Marker'
    metacommand = 0x7F

class SequencerSpecificEvent(MetaEvent):
    name = 'Sequencer Specific'
    metacommand = 0x7F


class TempoMap(list):
    def __init__(self, stream):
        self.stream = stream

    def add_and_update(self, event):
        self.add(event)
        self.update()

    def add(self, event):
        # get tempo in microseconds per beat
        tempo = event.mpqn
        # convert into milliseconds per beat
        tempo = tempo / 1000.0
        # generate ms per tick
        event.mpt = tempo / self.stream.resolution
        self.append(event)

    def update(self):
        self.sort()
        # adjust running time
        last = None
        for event in self:
            if last:
                event.msdelay = last.msdelay + \
                    int(last.mpt * (event.tick - last.tick))
            last = event

    def get_tempo(self, offset=0):
        last = self[0]
        for tm in self[1:]:
            if tm.tick > offset:
                return last
            last = tm
        return last

class EventStreamIterator(object):
    def __init__(self, stream, window):
        self.stream = stream
        self.trackpool = stream.trackpool
        self.window_length = window
        self.window_edge = 0
        self.leftover = None
        self.events = self.stream.iterevents()
        # First, need to look ahead to see when the
        # tempo markers end
        self.ttpts = []
        for tempo in stream.tempomap[1:]:
            self.ttpts.append(tempo.tick)
        # Finally, add the end of track tick.
        self.ttpts.append(stream.endoftrack.tick)
        self.ttpts = iter(self.ttpts)
        # Setup next tempo timepoint
        self.ttp = self.ttpts.next()
        self.tempomap = iter(self.stream.tempomap)
        self.tempo = self.tempomap.next()
        self.endoftrack = False

    def __iter__(self):
        return self

    def __next_edge(self):
        if self.endoftrack:
            raise StopIteration()
        lastedge = self.window_edge
        self.window_edge += int(self.window_length / self.tempo.mpt)
        if self.window_edge > self.ttp:
            # We're past the tempo-marker.
            oldttp = self.ttp
            try:
                self.ttp = self.ttpts.next()
            except StopIteration:
                # End of Track!
                self.window_edge = self.ttp
                self.endoftrack = True
                return
            # Calculate the next window edge, taking into
            # account the tempo change.
            msused = (oldttp - lastedge) * self.tempo.mpt
            msleft = self.window_length - msused
            self.tempo = self.tempomap.next()
            ticksleft = msleft / self.tempo.mpt
            self.window_edge = ticksleft + self.tempo.tick

    def next(self):
        ret = []
        self.__next_edge()
        if self.leftover:
            if self.leftover.tick > self.window_edge:
                return ret
            ret.append(self.leftover)
            self.leftover = None
        for event in self.events:
            if event.tick > self.window_edge:
                self.leftover = event
                return ret
            ret.append(event)
        return ret



"""
EventStream : Class used to describe a collection of MIDI Events.
"""
class EventStream(object):
    def __init__(self):
        self.format = 1
        self.trackcount = 0
        self.tempomap = TempoMap(self)
        self.curtrack = None
        self.trackpool = []
        self.tracklist = {}
        self.timemap = []
        self.endoftrack = None
        self.beatmap = []
        self.resolution = 220
        self.tracknames = {}

    def __set_resolution(self, resolution):
        # XXX: Add code to rescale notes
        assert(not self.trackpool)
        self.__resolution = resolution
        self.beatmap = []
        for value in BEATVALUES:
            self.beatmap.append(int(value * resolution))

    def __get_resolution(self):
        return self.__resolution
    resolution = property(__get_resolution, __set_resolution, None,
                                "Ticks per quarter note")

    def add_track(self):
        if self.curtrack == None:
            self.curtrack = 0
        else:
            self.curtrack += 1
        self.tracklist[self.curtrack] = []
        # Don't: when reading from a file trackcount comes from the header
        #self.trackcount += 1

    def get_current_track_number(self):
        return self.curtrack

    def get_track_by_number(self, tracknum):
        return self.tracklist[tracknum]

    def get_current_track(self):
        return self.tracklist[self.curtrack]

    def get_track_by_name(self, trackname):
        tracknum = self.tracknames[trackname]
        return self.get_track_by_number(tracknum)

    def replace_current_track(self, track):
        self.tracklist[self.curtrack] = track
        self.__refresh()

    def replace_track_by_number(self, tracknum, track):
        self.tracklist[tracknumber] = track
        self.__refresh()

    def replace_track_by_name(self, trackname, track):
        tracknum = self.tracklist[tracknum]
        self.repdeletelace_track_by_number(tracknum, track)

    def delete_current_track(self, track):
        del self.tracklist[self.curtrack]
        self.trackcount -= 1
        self.__refresh()

    def delete_track_by_number(self, tracknum):
        del self.tracklist[tracknum]
        self.trackcount -= 1
        self.__refresh()

    def delete_track_by_name(self, trackname, track):
        tracknum = self.tracklist[trackname]
        self.delete_track_by_number(tracknum, track)

    def add_event(self, event):
        self.__adjust_endoftrack(event)
        if not isinstance(event, EndOfTrackEvent):
            event.track = self.curtrack
            self.trackpool.append(event)
            self.tracklist[self.curtrack].append(event)
        if isinstance(event, TrackNameEvent):
            self.__refresh_tracknames()
        if isinstance(event, SetTempoEvent):
            self.tempomap.add_and_update(event)
            self.__refresh_timemap()
        else:
            if self.tempomap:
                tempo = self.tempomap.get_tempo(event.tick)
                event.adjust_msdelay(tempo)

    def get_tempo(self, offset=0):
        return self.tempomap.get_tempo(offset)

    def timesort(self):
        self.trackpool.sort()
        for track in self.tracklist.values():
            track.sort()
    
    def textdump(self):
        for event in self.trackpool:
            print("%s" % event)

    def __iter__(self):
        return iter(self.tracklist.values())

    def iterevents(self, mswindow=0):
        self.timesort()
        if mswindow:
            return EventStreamIterator(self, mswindow)
        return iter(self.trackpool)

    def __len__(self):
        print("LEN: len(self.tracklist): %d trackcount: %d" % \
              (len(self.tracklist), self.trackcount))
        
        assert(len(self.tracklist) == self.trackcount)
        return self.trackcount

    def __getitem__(self, intkey):
        return self.tracklist[intkey]

    def __refresh(self):
        self.__refresh_trackpool()
        self.__refresh_tempomap()
        self.__refresh_timemap()
        self.__refresh_tracknames()

    def __refresh_tracknames(self):
        self.tracknames = {}
        for tracknum in self.tracklist:
            track = self.tracklist[tracknum]
            for event in track:
                if isinstance(event, TrackNameEvent):
                    self.tracknames[event.data] = tracknum
                    break

    def __refresh_trackpool(self):
        self.trackpool = []
        for track in self.tracklist:
            track = self.tracklist[tracknum]
            for event in track:
                self.trackpool.append(event)
        self.trackpool.sort()

    def __refresh_tempomap(self):
        self.endoftrack = None
        self.tempomap = TempoMap(self)
        for event in self.trackpool:
            if isinstance(event, SetTempoEvent):
                self.tempomap.add(event)
            elif isinstance(event, EndOfTrackEvent):
                self.__adjust_endoftrack(event)
            self.tempomap.update()

    def __refresh_timemap(self):
        for event in self.trackpool:
            if not isinstance(event, SetTempoEvent):
                tempo = self.tempomap.get_tempo(event.tick)
                event.adjust_msdelay(tempo)

    def __adjust_endoftrack(self, event):
        if not self.endoftrack:
            if not event or not isinstance(event, EndOfTrackEvent):
                ev = EndOfTrackEvent()
                ev.tick = event.tick
                ev.track = self.curtrack
                self.endoftrack = ev
            else:
                self.endoftrack = event
            self.trackpool.append(self.endoftrack)
            self.tracklist[self.curtrack].append(self.endoftrack)
        else:
            self.endoftrack.tick = max(event.tick + 1, self.endoftrack.tick)
        if self.tempomap:
            tempo = self.tempomap.get_tempo(self.endoftrack.tick)
            self.endoftrack.adjust_msdelay(tempo)

class EventStreamReader(object):
    def __init__(self, instream, outstream):
        self.eventfactory = None
        self.parse(instream, outstream)

    def read(cls, instream, outstream=None):
        if not outstream:
            outstream = EventStream()
        cls(instream, outstream)
        return outstream
    read = classmethod(read)
    
    def parse(self, instream, outstream):
        self.midistream = outstream
        self.instream = instream
        if type(instream) in (type(b''), type(u'')):
            self.instream = open(instream, 'rb')
        self.parse_file_header()
        for track in range(self.midistream.trackcount):  
            trksz = self.parse_track_header()
            self.eventfactory = EventFactory()
            self.midistream.add_track()
            self.parse_track(trksz)
        
    def parse_file_header(self):
        # First four bytes are MIDI header
        magic = self.instream.read(4)
        if magic != b'MThd':
            raise TypeError("Bad header in MIDI file.")
        # next four bytes are header size
        # next two bytes specify the format version
        # next two bytes specify the number of tracks
        # next two bytes specify the resolution/PPQ/Parts Per Quarter
        # (in other words, how many ticks per quater note)
        data = unpack(">LHHH", self.instream.read(10))
        hdrsz = data[0]
        self.midistream.format = data[1]
        self.midistream.trackcount = data[2]
        self.midistream.resolution = data[3]
        # XXX: the assumption is that any remaining bytes
        # in the header are padding
        if hdrsz > DEFAULT_MIDI_HEADER_SIZE:
            self.instream.read(hdrsz - DEFAULT_MIDI_HEADER_SIZE)
            
    def parse_track_header(self):
        # First four bytes are Track header
        magic = self.instream.read(4)
        if magic != b'MTrk':
            raise TypeError("Bad track header in MIDI file: " + magic)
        # next four bytes are header size
        trksz = unpack(">L", self.instream.read(4))[0]
        return trksz

    def parse_track(self, trksz):
        track = iter(self.instream.read(trksz))
        while True:
            try:
                event = self.eventfactory.parse_midi_event(track)
                self.midistream.add_event(event)
            except StopIteration:
                break
                
def read_varlen(data):
    NEXTBYTE = 1
    value = 0
    while NEXTBYTE:
        chr = next_byte_as_int(data)
        # is the hi-bit set?
        if not (chr & 0x80):
            # no next BYTE
            NEXTBYTE = 0
        # mask out the 8th bit
        chr = chr & 0x7f
        # shift last value up 7 bits
        value = value << 7
        # add new value
        value += chr
    return value

read_midifile = EventStreamReader.read
