#!/bin/sh

VAR1="notFound"
VAR2="succesful"
VAR3="noHistory"
VAR4="unableToExecute"
VAR5="unableToFork"
VAR6="unsuccesfulDirectory"
PATH1="PATH TO THE FOLDER"

if [ "$*" = "$VAR1" ]; then 
    aplay "$PATH1/command_notfound.wav"
elif [ "$*" = "$VAR2" ]; then
    aplay "$PATH1/directory_succesful.wav"
elif [ "$*" = "$VAR3" ]; then
    aplay "$PATH1/no_history.wav"
elif [ "$*" = "$VAR4" ]; then
    aplay "$PATH1/unable_to_execute"
elif [ "$*" = "$VAR5" ]; then
    aplay "$PATH1/unable_to_fork.wav"
elif [ "$*" = "$VAR6" ]; then
    aplay "$PATH1/unsuccesful_directory.wav"
else
    echo "Unsuccesful"
fi
