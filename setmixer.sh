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

#Simple mixer control 'Left LR Playback Mixer Left',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 2
#  Mono: 0 [0%] [-99999.99dB]
#simple mixer control 'Left LR Playback Mixer Right',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 2
#  Mono: 0 [0%] [-99999.99dB]

# Set internal Left LR Playback Mixer
amixer cset numid=25 100%

# Set internal Right LR Playback Mixer
amixer cset numid=26 100%

# Now misc stuff we shouldn't have to touch

amixer set 'DSP Bypass' on

#Simple mixer control 'DAC Mono Stereo',0
#  Capabilities: enum
#  Items: 'Stereo' 'Mono Left Channel (L+R)' 'Mono Right Channel (L+R)' 'Mono (L+R)'
#  Item0: 'Stereo'

#Simple mixer control 'Digital',0
#  Capabilities: pvolume cvolume penum
#  Playback channels: Front Left - Front Right
#  Capture channels: Front Left - Front Right
#  Limits: Playback 0 - 255 Capture 0 - 255
#  Front Left: Playback 255 [100%] [0.00dB] Capture 255 [100%] [0.00dB]
#  Front Right: Playback 255 [100%] [0.00dB] Capture 255 [100%] [0.00dB]

# Bias, boost, filter Settings

#Simple mixer control 'Headphone Bias',0
#Items: 'Normal operation' 'Extreme power saving' 'Enhanced performance' 'Power saving'
# Item0: 'Normal operation'

#Simple mixer control 'Mic Bias Mode',0
#  Items: 'Normal operation' 'High performance'
#  Item0: 'Normal operation'

#Simple mixer control 'Playback Bias',0
#  Items: 'Normal operation' 'Enhanced performance' 'Power saving'
#  Item0: 'Normal operation'

#Simple mixer control 'Capture Bias',0
#  Items: 'Normal operation' 'Enhanced performance' 'Power saving'
#  Item0: 'Normal operation'

#Simple mixer control 'Capture Boost',0
#  Items: 'Normal operation' 'Boost Level 1' 'Boost Level 2' 'Boost Level 3'
#  Item0: 'Normal operation'

#Simple mixer control 'Playback De-emphasis',0
#  Capabilities: pswitch pswitch-joined penum
#  Playback channels: Mono
#  Mono: Playback [off]

#Simple mixer control 'ADC Bias',0
#  Items: 'Normal operation' 'Extreme power saving' 'Enhanced performance' 'Power saving'
#  Item0: 'Normal operation'

#Simple mixer control 'ADC High Pass Filter',0
#  Capabilities: pswitch pswitch-joined penum
#  Playback channels: Mono
#  Mono: Playback [off]

#Simple mixer control 'DAC Bias',0
#  Items: 'Normal operation' 'Extreme power saving' 'Enhanced performance' 'Power saving'
#  Item0: 'Normal operation'



#Simple mixer control 'Left Playback Mixer Aux Bypass',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 8
#  Mono: 0 [0%] [-99999.99dB]
#Simple mixer control 'Left Playback Mixer Left Bypass',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 8
#  Mono: 0 [0%] [-99999.99dB]

amixer set 'Left Playback Mixer Left DAC' on
#Simple mixer control 'Left Playback Mixer Right Bypass',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 8
#  Mono: 0 [0%] [-99999.99dB]
amixer set 'Left Playback Mixer Right DAC' on

#Simple mixer control 'Right LR Playback Mixer Left',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 2
#  Mono: 0 [0%] [-99999.99dB]
#Simple mixer control 'Right LR Playback Mixer Right',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 2
#  Mono: 0 [0%] [-99999.99dB]

#Simple mixer control 'Right Playback Mixer Aux Bypass',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 8
#  Mono: 0 [0%] [-99999.99dB]
#Simple mixer control 'Right Playback Mixer Left Bypass',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 8
#  Mono: 0 [0%] [-99999.99dB]
amixer set 'Right Playback Mixer Left DAC' on
#Simple mixer control 'Right Playback Mixer Right Bypass',0
#  Capabilities: volume volume-joined penum
#  Playback channels: Mono
#  Capture channels: Mono
#  Limits: 0 - 8
#  Mono: 0 [0%] [-99999.99dB]
amixer set 'Right Playback Mixer Right DAC' on
