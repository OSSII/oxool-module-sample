read -p "Please input module name:" project_name

cd .. && mkdir $project_name && cp -r sample/* $project_name && cd $project_name
find . \( -type d -name .git -prune \) -o -type f -print0 | xargs -0 sed -i "s/sample/$project_name/g"
find . -name 'sample*' -type f | while read filename ;do mv $filename ${filename/sample/$project_name} ;done