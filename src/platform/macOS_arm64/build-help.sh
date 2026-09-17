#!/usr/bin/env bash
set -euo pipefail

repository_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
source_root="$repository_root/src/ATHelpFile/source"
help_output=${1:-"$repository_root/out/macOS_arm64/Help"}

if ! command -v xsltproc >/dev/null 2>&1; then
	printf 'xsltproc is required to build Altirra help.\n' >&2
	exit 1
fi

case $help_output in
	''|/)
		printf 'Unsafe help output path: %s\n' "$help_output" >&2
		exit 1
		;;
esac

source_hash=$(
	{
		find "$source_root" -type f -print |
			LC_ALL=C sort |
			while IFS= read -r source_file; do
				shasum -a 256 "$source_file"
			done
		shasum -a 256 "${BASH_SOURCE[0]}"
	} |
		shasum -a 256 |
		awk '{print $1}'
)

if [[ -s $help_output/index.html
	&& -s $help_output/contents.html
	&& -f $help_output/.source.sha256
	&& $(<"$help_output/.source.sha256") == "$source_hash" ]]; then
	printf 'CACHED  Altirra HTML help\n'
	exit 0
fi

mkdir -p "$(dirname "$help_output")"
temporary_output=$(mktemp -d "${help_output}.tmp.XXXXXX")
cleanup() {
	rm -rf "$temporary_output"
}
trap cleanup EXIT

topic_count=0
for source_file in "$source_root"/*.xml; do
	filename=${source_file##*/}
	if [[ $filename == toc.xml ]]; then
		continue
	fi

	stylesheet=$(sed -n 's/.*xml-stylesheet.*href="\([^"]*\)".*/\1/p' "$source_file" | head -1)
	if [[ -z $stylesheet || ! -f $source_root/$stylesheet ]]; then
		printf 'Missing XSL stylesheet declaration in %s.\n' "$source_file" >&2
		exit 1
	fi

	xsltproc --nonet \
		--output "$temporary_output/${filename%.xml}.html" \
		"$source_root/$stylesheet" \
		"$source_file"
	topic_count=$((topic_count + 1))
done

xsltproc --nonet \
	--output "$temporary_output/contents.html" \
	"$source_root/toc-web.xsl" \
	"$source_root/toc.xml"

cp "$source_root/layout.css" "$temporary_output/layout.css"
cp -R "$source_root/pics" "$temporary_output/pics"

if (( topic_count == 0 )) || [[ ! -s $temporary_output/index.html ]]; then
	printf 'No Altirra help topics were generated.\n' >&2
	exit 1
fi

validate_reference() {
	local source_file=$1
	local reference=$2
	local target

	case $reference in
		''|'#'*|http:*|https:*|mailto:*|javascript:*|data:*)
			return
			;;
	esac

	target=${reference%%#*}
	target=${target%%\?*}
	if [[ ! -e "$(dirname "$source_file")/$target" ]]; then
		printf 'Broken help reference in %s: %s\n' "${source_file#$temporary_output/}" "$reference" >&2
		exit 1
	fi
}

while IFS= read -r html_file; do
	while IFS= read -r attribute; do
		validate_reference "$html_file" "${attribute#href=\"}" 
	done < <(grep -Eo 'href="[^"]+"' "$html_file" | sed 's/"$//' || true)

	while IFS= read -r attribute; do
		validate_reference "$html_file" "${attribute#src=\"}"
	done < <(grep -Eo 'src="[^"]+"' "$html_file" | sed 's/"$//' || true)
done < <(find "$temporary_output" -type f -name '*.html' | LC_ALL=C sort)

printf '%s\n' "$source_hash" > "$temporary_output/.source.sha256"
rm -rf "$help_output"
mv "$temporary_output" "$help_output"
trap - EXIT

printf 'Generated Altirra HTML help: %s topics.\n' "$topic_count"
