to=~/git/valvecontrol/firmware/app/
from=~/git/testapp/build/
rsync -zarv --delete --include="*/" --include="*.gz" --include="*.json" --include="*.png" --include="*.ico"--include="*.txt"  --exclude="*"  "$from" "$to"
