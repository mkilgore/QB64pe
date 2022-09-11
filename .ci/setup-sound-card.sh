#!/bin/bash

modinfo soundcore

cat << EOF > $HOME/.asoundrc
       pcm.dummy {
          type hw
          card 0
       }
       
       ctl.dummy {
          type hw
          card 0
       }
EOF

chmod go+r $HOME/.asoundrc

sudo cat << EOF >> /etc/modprobe.d/alsa.conf
# ALSA portion
alias char-major-116 snd
alias snd-card-0 snd-dummy
# module options should go here
       
# OSS/Free portion
alias char-major-14 soundcore
alias sound-slot-0 snd-card-0

# OSS/Free portion - card #1
alias sound-slot-0 snd-card-0
alias sound-service-0-0 snd-mixer-oss
alias sound-service-0-1 snd-seq-oss
alias sound-service-0-3 snd-pcm-oss
alias sound-service-0-8 snd-seq-oss
alias sound-service-0-12 snd-pcm-oss
EOF

sudo modprobe snd-dummy
