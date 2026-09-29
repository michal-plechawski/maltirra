param(
	[Parameter(Mandatory = $true)]
	[string] $MarkerPath
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $MarkerPath -PathType Leaf)) {
	Write-Host 'No cache commit marker was restored; leaving source timestamps unchanged.'
	exit 0
}

$cacheCommit = (Get-Content -LiteralPath $MarkerPath -Raw).Trim()
if ($cacheCommit -notmatch '^[0-9a-fA-F]{40}$') {
	Write-Warning "Ignoring invalid cache commit marker: $cacheCommit"
	exit 0
}

& git cat-file -e "$cacheCommit`^{commit}" 2>$null
if ($LASTEXITCODE -ne 0) {
	Write-Warning "Cached commit $cacheCommit is not available in the checkout; leaving source timestamps unchanged."
	exit 0
}

# Restored compiler outputs retain their original mtimes. Make every unchanged
# tracked input older than those outputs, then make inputs changed since the
# cached commit newer. This lets MSBuild reuse the cache without ever hiding a
# source or project change behind an old commit timestamp.
$oldTimestamp = [DateTime]::SpecifyKind([DateTime]'2000-01-01T00:00:00', [DateTimeKind]::Utc)
$newTimestamp = [DateTime]::UtcNow
$trackedCount = 0
$changedCount = 0

$trackedFiles = @(& git ls-files)
if ($LASTEXITCODE -ne 0) {
	throw 'git ls-files failed while preparing the MSBuild cache.'
}

foreach ($path in $trackedFiles) {
	if (Test-Path -LiteralPath $path -PathType Leaf) {
		[IO.File]::SetLastWriteTimeUtc($path, $oldTimestamp)
		++$trackedCount
	}
}

$changedFiles = @(& git diff --name-only --no-renames $cacheCommit HEAD --)
if ($LASTEXITCODE -ne 0) {
	throw "git diff failed while comparing the cache with $cacheCommit."
}

foreach ($path in $changedFiles) {
	if (Test-Path -LiteralPath $path -PathType Leaf) {
		[IO.File]::SetLastWriteTimeUtc($path, $newTimestamp)
		++$changedCount
	}
}

Write-Host "Prepared incremental MSBuild cache from $cacheCommit ($trackedCount tracked files, $changedCount changed files)."
