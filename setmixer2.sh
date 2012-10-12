# amixer script for microburst sound device
# from Steve Conklin
#
# Configure the chip switches to enable things
# Enable the Headphone Output
amixer cset numid=28 1
# Enable the Line Output
amixer cset numid=30 1
# Enable the Mono Output
amixer cset numid=24 1
# Set Mono Output to 0 dB
amixer cset numid=25 1

# Set the headphone output to 50% Volume
amixer cset numid=27 50%
# Set the line output to 50% volume
amixer cset numid=29 50%
# Set the mono output to 50% volume
amixer cset numid=23 50%

# Enable Capture Input
amixer cset numid=11 1
# Set Capture Gain to +20 dB
amixer cset numid=12 2
# Set Capture Volume to 50%
amixer cset numid=10 50%

# Internal mixers

# Set internal Left LR Playback Mixer Left to 50%
amixer cset numid=39 100%

# Set internal Right LR Playback Mixer Right to 50%
amixer cset numid=38 100%

# Set DSP Bypass on Playback and Capture

amixer cset numid=8 1
amixer cset numid=9 1

# Enable Left DAC to Left and Right DAC to Right

amixer cset numid=41 0
amixer cset numid=42 1

amixer cset numid=46 1
amixer cset numid=47 0

