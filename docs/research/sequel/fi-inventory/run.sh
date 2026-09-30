#!/bin/zsh
source ~/VulkanSDK/1.4.363.0/setup-env.sh >/dev/null
cd /Users/matteo/dev/kya/.claude/worktrees/agent-a0cbedbfa6600eab6/bin/MAC
export KYA_LOG_LEVEL=${KYA_LOG_LEVEL:-info}
./Kya_Debug > $1/run.log 2>&1
echo exit $?
