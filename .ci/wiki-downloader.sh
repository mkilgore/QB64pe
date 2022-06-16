#!/bin/bash

base="https://qb64phoenix.com/qb64wiki/"
allPagesApi="api.php?action=query&list=allpages&aplimit=max&format=json"
getPageContext="api.php?action=parse&prop=wikitext&formatversion=2&format=json&disabletoc=true"
expandTemplates="api.php?action=expandtemplates&prop=wikitext&format=json&disabletoc=true"

jsonResponse=$(curl -L "$base$allPagesApi")

echo "$jsonResponse"

echo "$jsonResponse" | jq -c ".query.allpages[]" | while read line
do
    pageid=$(echo "$line" | jq -r ".pageid")
    pageTitle=$(echo "$line" | jq -r ".title")

    if [ "$pageTitle" == '*' ]; then
        continue;
    fi

    if [ "$pageTitle" == '/' ]; then
        continue;
    fi

    if [ "$pageTitle" == '\' ]; then
        continue;
    fi

    echo "PageID: $pageid"
    echo "PageTitle: $pageTitle"

    (
        wikitext=$(curl -L "$base$getPageContext&pageid=$pageid" | \
            jq -r ".parse.wikitext" | \
            sed 's/\\n/\n/g')

        expanded=$(curl "$base$expandTemplates" --data-urlencode "title=$pageTitle" --data-urlencode "text=$wikitext" | \
            jq -r ".expandtemplates.wikitext" | \
            sed 's/\\n/\n/g' | \
            sed 's/|  __TOC__/__NOTOC__/g' | \
            sed -E 's/\[\[([^|]*)\|<span style="color:\#87cefa;">[^<]*<\/span>\]\]/\1/g')

        echo "$expanded" > "./wiki/$pageTitle.mediawiki"
    ) &
done

wait

cat "./wiki/\$ASSERTS.mediawiki"

cd ./wiki

git config --local user.email "41898282+github-actions[bot]@users.noreply.github.com"
git config --local user.name "github-actions[bot]"
git add .

if ! git diff --cached --quiet; then
    echo "Pushing updated wiki"
    git commit -m 'Automatic update of Wiki'
    git push
else
    echo "No changes to wiki"
fi
