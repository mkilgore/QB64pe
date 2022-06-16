#!/bin/bash

base="https://qb64phoenix.com/qb64wiki/"
allPagesApi="api.php?action=query&list=allpages&aplimit=100&format=json"
getPageContext="api.php?action=parse&prop=wikitext&formatversion=2&format=json&disabletoc=true"
expandTemplates="api.php?action=expandtemplates&prop=wikitext&format=json&disabletoc=true"

continueTitle="ASSERT"

while :
do

    if [ -z "$continueTitle" ]; then
        jsonResponse=$(curl -L "$base$allPagesApi")
    else
        jsonResponse=$(curl -L "$base$allPagesApi" --data-urlencode "apcontinue=$continueTitle")
    fi

    echo "$jsonResponse"

    continueTitle=$(echo "$jsonResponse" | jq -r ".continue.apcontinue")
    echo "Continue title: $continueTitle"

    i=0

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
            wikitext=$(curl -s -L "$base$getPageContext&pageid=$pageid" | \
                jq -r ".parse.wikitext" | \
                sed 's/\\n/\n/g')

            displayTitle=$(echo "$wikitext" | grep "DISPLAYTITLE" | sed -E 's/\{\{DISPLAYTITLE:([^}]+)\}\}/\1/g')
            echo "DISPLAY TITLE: $displayTitle"

            expanded=$(curl -s "$base$expandTemplates" --data-urlencode "title=$pageTitle" --data-urlencode "text=$wikitext" | \
                jq -r ".expandtemplates.wikitext" | \
                sed 's/\\n/\n/g' | \
                sed 's/|  __TOC__/__NOTOC__/g' | \
                sed -E 's/\[\[([^|]*)\|<span style="color:\#87cefa;">[^<]*<\/span>\]\]/\1/g')

            if [ ! -z "$displayTitle" ]; then
                pageTitle="$displayTitle"
            fi

            echo "$expanded" > "./wiki/$pageTitle.mediawiki"
        ) &

        i=$((i + 1))
        if [ "$i" == "10" ]; then
            echo "Wait"
            wait
            i=0
        fi
    done

    wait

    [ "$continueTitle" == "null" ] && break
done

cp "./wiki/Main\ Page.mediawiki" ./wiki/Home.mediawiki

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
