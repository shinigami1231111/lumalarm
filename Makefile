# Convenience wrappers around scripts/ci-local.sh — the local mirror of
# .github/workflows/ci.yml. bash is invoked explicitly so a fresh clone
# without the executable bit still works.

.PHONY: ci ci-fast clean-ci

ci:
	bash scripts/ci-local.sh

ci-fast:
	bash scripts/ci-local.sh --no-lint

clean-ci:
	rm -rf build-local-ci
