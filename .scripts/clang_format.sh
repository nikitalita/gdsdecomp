#!/bin/sh

CLANG_FORMAT=clang-format-6.0

# If this wasn't triggered by a pull request...
if [ -z "$GITHUB_HEAD_REF" ] && [ -n "$GITHUB_REF" ]; then
    # Check the whole commit range against $GITHUB_REF, the base merge branch
	echo "Checking all of $GITHUB_REF"
    RANGE="$(git rev-parse $GITHUB_REF) HEAD"
# If this was triggerd by a pull request...
else
    # Test only the last commit
    RANGE=HEAD
fi
echo "Checking $RANGE"

FILES=$(git diff-tree --no-commit-id --name-only -r $RANGE | grep -E "\.(c|h|cpp|hpp|cc|hh|cxx|m|mm|inc)$")
echo "Checking files:\n$FILES"

# create a random filename to store our generated patch
prefix="static-check-clang-format"
suffix="$(date +%s)"
patch="/tmp/$prefix-$suffix.patch"

for file in $FILES; do
    "$CLANG_FORMAT" -style=file "$file" | \
        diff -u "$file" - | \
        sed -e "1s|--- |--- a/|" -e "2s|+++ -|+++ b/$file|" >> "$patch"
done

# if no patch has been generated all is ok, clean up the file stub and exit
if [ ! -s "$patch" ] ; then
    printf "Files in this commit comply with the clang-format rules.\n"
    rm -f "$patch"
    exit 0
fi

# a patch has been created, notify the user and exit
printf "\n*** The following differences were found between the code to commit "
printf "and the clang-format rules:\n\n"
cat "$patch"
printf "\n*** Aborting, please fix your commit(s) with 'git commit --amend' or 'git rebase -i <hash>'\n"
exit 1
