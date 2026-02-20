# KNOWN ISSUES

## Sync-required bucket (breaking / coordinated changes)

	Replacing shell runner with true process backends (CreateProcess / fork+exec with real pid/timeout/kill/separate stderr).
	Renaming/removing legacy API surface (RunProcess, stdout_text/stderr_text naming changes, or removing fields like pid/timed_out).
	Contract-level payload shape changes beyond current fields (if you want explicit exit code/termination metadata in job response).
	
## Deferred bucket (non-obvious / larger-scope)

	Structured diagnostics for getaddrinfo/socket errors in port_probe (needs error plumbing design).
	POSIX signaled-exit integration test in Linux CI lane (Windows environment can’t validate this path).
	If you want, next I can draft a concrete sync plan PR checklist for both 40318-SOFT and test-fixture-data-bridge tied to the breaking bucket.
	

more random remaining items

	Add explicit process-runner naming migration:
	introduce RunShellProcess(...) alias now, mark RunProcess(...) deprecated in comments, then remove in a later release.
	Add observable exit code to job payload if you want strong decode tests:
	include rc in action job/result so tests can assert exact 7 and POSIX signal mapping.
	Approve policy/design change for real backend work:
	current guardrails block platform-specific APIs/files; update policy first.
	Implement true backend after policy change:
	Windows: CreateProcess + pipes + wait/timeout/terminate
	POSIX: fork/exec + pipe/dup2 + waitpid/timeout/kill
	Keep shell runner as fallback and wire capability flags from backend at runtime.