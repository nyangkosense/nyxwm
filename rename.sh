#!/bin/bash

# Check if a new name is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <new_name>"
    echo "Example: $0 nyxwm"
    exit 1
fi

NEW_NAME=$1
OLD_NAME="nyxwm"

# Function to rename files
rename_files() {
    find . -depth -name "*$OLD_NAME*" -execdir bash -c 'mv "$1" "${1//'$OLD_NAME'/'$NEW_NAME'}"' _ {} \;
}

# Function to replace content in files
replace_content() {
    grep -rl "$OLD_NAME" . | xargs sed -i "s/$OLD_NAME/$NEW_NAME/g"
}

# Rename files
echo "Renaming files..."
rename_files

# Replace content in files
echo "Replacing content in files..."
replace_content

# Update Makefile
echo "Updating Makefile..."
sed -i "s/$OLD_NAME/$NEW_NAME/g" Makefile

echo "Renaming complete. Project renamed from $OLD_NAME to $NEW_NAME."
echo "Please review the changes and update any remaining occurrences manually if necessary."
