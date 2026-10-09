# Cutting a release

Start at [AGENTS.md](../../AGENTS.md).

End users install from the dist tarball attached to a GitHub release (see
README.md), *not* from GitHub's auto-generated "Source code" archives —
those are plain git exports with no `configure` in them.

`.github/workflows/release.yml` handles that: on a pushed `v*` tag it
builds the tarball, verifies it unpacks and builds with no Autotools
present, attests where it was built, checks that attestation as a user
would (`gh attestation verify`), then uploads it with its `SHA256SUMS` —
*creating* the release if the tag has none yet.

The attestation is a Sigstore-signed SLSA provenance statement, stored
with the repository (not as a release asset): it says the tarball with
this digest was built by `release.yml`, in this repository, from this
commit. It needs the job's `id-token: write` and `attestations: write`.

A pull request that changes the workflow runs it (its `tarball` job):
it builds, checks the tarball, attests and verifies, and uploads
nothing. Pull requests from Dependabot (which changes it every time it
updates an action) or from a fork have no OIDC token to sign with and
skip the two attestation steps. To run it by hand on a branch, `publish` off:

```bash
gh workflow run release.yml --ref <branch> -f tag=vX.Y.Z -f publish=false
```

A release is prepared on a branch like any change (`release-X.Y.Z`,
with an issue of its own), then tagged on `main` once merged.

1. **Version**: set `AC_INIT` in `configure.ac` to `X.Y.Z`, dropping the
   `-dev`. In `CHANGELOG.md`, turn `[Unreleased]` into
   `[X.Y.Z] - YYYY-MM-DD` (and its link at the bottom); in `CITATION.cff`,
   set `version` and `date-released` to the same. `test_release_files.sh`
   fails until the three agree.
2. **Man pages**: if one changed since the last release, set the date in
   its `.TH` line (`man/*.1.in`) to today.
3. **README**: replace the Quick start output with what the new version
   really prints (`bmb_dgemm -m 512:2048`). If the report's look changed,
   redo `doc/images` (see [The README's images](report.md#the-readmes-images)).
4. **Check**: `make distcheck`, and the browser tests in Firefox
   (`BMB_BROWSER=firefox make check`); commit, push the branch, open the
   pull request, and merge it once every required check passes.
5. **Tag and publish**, from the merged `main`:

   ```bash
   git switch main && git pull
   git tag -a vX.Y.Z -m "blas-microbenchmark X.Y.Z"
   git push origin vX.Y.Z
   # wait for the workflow (~30s), then write the release notes
   gh release edit vX.Y.Z --title vX.Y.Z --notes-file notes.md
   ```

6. **Verify the tarball** the release carries: download it with its
   `SHA256SUMS` and run `sha256sum -c SHA256SUMS` and `gh attestation
   verify <tarball> --repo MathieuCAISSA/blas-microbenchmark`, then
   `./configure --prefix=...`, `make check`, `make install` — the man
   pages included — on a machine with no Autotools if you can.
7. **Back to development**: on another short branch and pull request,
   set `AC_INIT` to the next minor version with `-dev` (`X.(Y+1).0-dev`),
   add an empty `[Unreleased]` section to `CHANGELOG.md`, and add the
   release to the Spack package with the checksum the release published
   (see [After a release](packaging.md#after-a-release)); the `spack`
   job then builds it from GitHub.

Note the order: **edit the notes, don't create the release**. The workflow
gets there within about half a minute of the tag push and creates it with
`--generate-notes`, so a `gh release create` afterwards just fails with
"Release.tag_name already exists".

Re-run the upload for an existing tag with
`gh workflow run release.yml -f tag=vX.Y.Z`.
