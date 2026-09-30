#!/bin/zsh
source ~/VulkanSDK/1.4.363.0/setup-env.sh >/dev/null
cd /Users/matteo/dev/kya/.claude/worktrees/agent-a0cbedbfa6600eab6/bin/MAC
echo '{"Auto Buy White Bracelet":false,"Auto Load Level ID":'${2:-8}'}' > settings.json
lldb --batch -o "process handle SIGUSR1 SIGUSR2 SIGPIPE -n true -p true -s false" -o run -o "bt 25" -o "frame variable" -o quit -- ./Kya_Debug > $1/dbg.log 2>&1
echo exit $?
