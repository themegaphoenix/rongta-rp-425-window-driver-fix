# Publish a release

The `Tagged release` workflow builds and runs native checks, packages the tested
DLL and installation files, then publishes an **experimental pre-release** with
the ZIP and SHA-256 checksum. It uses the repository's built-in `GITHUB_TOKEN`;
no personal access token or signing-policy change is required.

Before tagging, review known limits, update `docs/release-notes.md` if needed,
and confirm CI is passing on the commit you intend to release. Tag only reviewed
code: the release workflow grants that tagged job permission to publish assets.

For the first release, run in a clean clone:

```powershell
git switch main
git pull --ff-only
git tag -a v0.1.0 -m 'Experimental native Bluetooth monitor'
git push origin v0.1.0
```

Use a new version for later releases, such as `v0.1.1` or `v0.2.0-rc.1`. Supported
tag names are `vMAJOR.MINOR.PATCH` with an optional prerelease suffix. The workflow
runs on the pushed tag's commit, which must contain the release workflow.

Watch **Actions → Tagged release**, then check **Releases** for its published
pre-release. If building/tests fail, no release is created. Fix the issue and
use a new version tag; do not silently move an existing release tag.

For a local package smoke test without publication:

```powershell
./scripts/build.ps1
./scripts/package.ps1 -Version local
```

Ordinary branch CI now uploads the same installation bundle as an Actions
artifact. Actions artifacts and GitHub Releases are separate download surfaces.
