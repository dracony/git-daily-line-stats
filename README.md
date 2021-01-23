# git-daily-line-stats
Simple utility to print lines committed daily per dev

To make the best effort of giving accurate results this tool:

* Will skip commits with multiple parents (those are usually merge commits)
* Will consider commits with same author and commit timestamp to be equal across branches even if their commit ids differ. *This is because moving commits to a different branch (via rebase or cherry-pick) generates a new commit id.*

Output for this repo

```
2021/01/23:
        Dracony:
                Commits: 2
                Added lines : 20
                Removed lines : 40
```
