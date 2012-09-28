# amixer script for microburst sound device
# from Steve Conklin
#
# Configure the chip switches to enable things
# Enable the Headphone Output
amixer cset numid=16 1
# Enable the Line Output
amixer cset numid=18 1
# first, the outputs
amixer set 'Lineout' 50%
amixer set 'Headphone' 50%

# Now the inputs
amixer set 'Aux' 0%

amixer set 'Input 1' 0%
amixer set 'Input 2' 0%
amixer set 'Input 3' 0%
amixer set 'Input 4' 0%

# Internal mixers

# Set internal Left LR Playback Mixer
amixer cset numid=25 100%

# Set internal Right LR Playback Mixer
amixer cset numid=26 100%

# Set DSP Bypass on Playback and Capture

amixer cset numid=8 1
amixer cset numid=9 1

# Enable Left and Right DAC

amixer cset numid=33 1
amixer cset numid=34 1

amixer cset numid=28 1
amixer cset numid=29 1

