# RecoverySuite Development Rules

This document contains permanent rules that govern every future coding session for the RecoverySuite project. All contributors must follow these rules to maintain code quality, consistency, and project health.

## Project Structure

1. **Directory Organization**: Follow the structure defined in `docs/architecture/project_structure.md`
2. **No Root Clutter**: Only essential files in repository root (README.md, license, configuration files)
3. **Documentation Location**: All documentation in `/docs/` directory
4. **Source Code Location**: Implementation code in appropriately named directories under root
5. **Build Artifacts**: Never commit build artifacts, object files, or executables
6. **Third-Party Code**: Place in `/ThirdParty/` directory with clear licensing information

## Coding Standards (C++20)

1. **Language Version**: Use only standard C++20 features
2. **No Extensions**: Avoid compiler-specific extensions unless absolutely necessary and well-isolated
3. **RAII Everywhere**: Resource Acquisition Is Initialization must be used for all resource management
4. **Smart Pointers**: Use `std::unique_ptr` for exclusive ownership, `std::shared_ptr` for shared ownership
5. **No Raw Owning Pointers**: Never use raw pointers for ownership; use smart pointers instead
6. **No Global State**: Avoid global variables; use dependency injection or service locators
7. **Const Correctness**: Use `const` extensively for variables, parameters, and methods that don't modify state
8. **Error Handling**: Use exceptions for error conditions; never ignore error return values
9. **No Magic Numbers**: Use named constants with `constexpr` or `enum class`
10. **Header Hygiene**: Minimize includes in headers; use forward declarations when possible
11. **Namespace Usage**: Use project namespace (e.g., `recoverysuite`) to avoid pollution
12. **Lifetime Safety**: Prevent dangling references and use-after-free through careful design

## Naming Conventions

1. **Files and Directories**: Use snake_case (e.g., `file_system_parser.h`)
2. **Classes and Structs**: Use PascalCase (e.g., `NtfsFileSystem`)
3. **Functions and Methods**: Use snake_case (e.g., `parse_boot_sector()`)
4. **Variables**: Use snake_case (e.g., `bytes_per_sector`)
5. **Constants**: Use UPPER_SNAKE with `k` prefix for scoped constants (e.g., `kDefaultBufferSize`)
6. **Enumerations**: Use PascalCase for enum name, UPPER_SNAKE for values (e.g., `FileSystemType::NTFS`)
7. **Templates**: Use PascalCase with `_t` suffix for type aliases (e.g., `SectorBuffer_t`)
8. **Macros**: Avoid when possible; if used, use UPPER_SNAKE (e.g., `RECOVERYSUITE_ASSERT`)
9. **Interfaces**: Suffix with `_Interface` (e.g., `FileSystem_Interface`)
10. **Abstract Base Classes**: Suffix with `_Base` (e.g., `Allocator_Base`)

## Error Handling

1. **Exceptions**: Use C++ exceptions for error conditions; `noexcept` only for truly non-throwing functions
2. **Error Types**: Define specific exception types inheriting from `std::exception`
3. **No Error Ignoring**: Always check return values from functions that can fail
4. **Resource Cleanup**: Use RAII; destructors must clean up resources even during stack unwinding
5. **Assertions**: Use `assert()` for preconditions and internal invariants; provide descriptive messages
6. **Error Context**: Include relevant context in error messages (file, line, operation being performed)
7. **Logging**: Log error conditions at appropriate level before throwing/returning error
8. **Recovery vs Fatal**: Distinguish between recoverable errors and fatal errors requiring shutdown
9. **No Broad Catches**: Catch specific exception types; avoid `catch (...)`
10. **Exception Safety**: Provide basic exception guarantee; strongly consider strong guarantee

## Logging

1. **Logging Framework**: Use a high-performance logging library (e.g., spdlog)
2. **Log Levels**: Use appropriate levels (trace, debug, info, warn, error, critical)
3. **Performance Sensitive Code**: Minimize logging in hot paths; use conditional logging
4. **Context Information**: Include thread ID, timestamp, and relevant context in log messages
5. **No Sensitive Data**: Never log passwords, keys, or personally identifiable information
6. **Log Rotation**: Implement log rotation for long-running processes
7. **Async Logging**: Use asynchronous logging where performance is critical
8. **Log Format**: Consistent, parseable format (JSON or structured text preferred)
9. **Log Ownership**: Each module responsible for logging its own operations
10. **Startup/Shutdown**: Log major lifecycle events at info level or higher

## Documentation

1. **Every Public Interface**: Document all public classes, methods, and functions
2. **Design Decisions**: Document non-obvious design decisions with comments
3. **Limitations**: Document known limitations and assumptions
4. **Thread Safety**: Clearly document thread safety guarantees
5. **Exception Safety**: Document exception safety level (no-throw, strong, basic)
6. **Performance Characteristics**: Document time/space complexity when relevant
7. **Examples**: Provide usage examples for complex APIs
8. **Documentation Generation**: Design documentation to be extractable by tools like Doxygen
9. **Keep Updated**: Update documentation when changing code; outdated documentation is worse than none
10. **Architecture Docs**: Update architecture documents when making significant structural changes

## Testing

1. **Test-First Approach**: Write unit tests before implementing functionality when practicable
2. **Test Coverage**: Aim for high test coverage; critical paths must be well-tested
3. **Test Naming**: Use descriptive names that indicate what is being tested
4. **Isolation**: Unit tests should test one thing in isolation; use mocks/fakes for dependencies
5. **Test Location**: Place tests in `/Tests/` directory mirroring source structure
6. **Test Independence**: Each test should be able to run independently and in any order
7. **Test Cleanup**: Tests must leave no lasting side effects; clean up resources
8. **Edge Cases**: Test boundary conditions, invalid inputs, and error cases
9. **Performance Tests**: Include performance benchmarks for algorithms where relevant
10. **Continuous Integration**: All tests must pass on CI before merging
11. **Test Documentation**: Document what each test suite covers
12. **No Flaky Tests**: Fix or eliminate tests that pass/fail non-deterministically

## Git Workflow

1. **No Direct Commits to Main**: Always work in feature branches
2. **Branch Naming**: Use prefixes: `feat/`, `fix/`, `refactor/`, `docs/`, `test/`, `chore/`
3. **Commit Messages**: Follow Conventional Commits format: `type(scope): description`
   - Examples: `feat(nfts): add mft parser`, `fix(hex_editor): resolve off-by-one error`
4. **Atomic Commits**: Each commit should represent a single logical change
5. **Meaningful Messages**: Explain why the change was made, not just what was changed
6. **References**: Include issue/ticket numbers when relevant
7. **Pull Process**: 
   - Fetch latest main
   - Create feature branch
   - Implement and test
   - Commit with good messages
   - Push branch
   - Open pull request
8. **Code Review**: All changes require code review before merging
9. **Rebase Preference**: Prefer rebasing over merging for clean history
10. **Signed Commits**: Use GPG signing for commits when available
11. **Worktree Isolation**: Follow worktree isolation procedures for parallel development
12. **No Secrets**: Never commit passwords, keys, or credentials

## Security

1. **Input Validation**: Validate all inputs from external sources (files, network, user input)
2. **Buffer Overflows**: Use bounds-checked containers and functions; avoid raw arrays when possible
3. **Format Strings**: Never use user-controlled data as format strings
4. **Command Injection**: Avoid shell commands; if necessary, sanitize and use proper escaping
5. **Information Leakage**: Be careful about error messages that might reveal sensitive information
6. **Memory Safety**: Prevent use-after-free, double-free, buffer overflows
7. **Secrets Management**: Never store secrets in source code; use secure vaults or environment variables
8. **Privilege Minimization**: Run with minimum necessary privileges
9. **Security Reviews**: Treat security as a first-class concern in design and code review
10. **Third-Party Vetting**: Vet third-party libraries for known vulnerabilities
11. **Defensive Programming**: Assume inputs are malicious; validate and sanitize

## Performance

1. **Measure First**: Profile before optimizing; optimize based on measurements
2. **Algorithmic Complexity**: Choose appropriate algorithms and data structures for expected usage
3. **Memory Allocation**: Minimize dynamic allocation in hot paths; use object pools
4. **Cache Locality**: Design for cache efficiency; access memory sequentially when possible
5. **Copy Avoidance**: Use move semantics and references to avoid unnecessary copying
6. **Lazy Initialization**: Delay expensive initialization until actually needed
7. **Concurrency**: Use threading where appropriate; avoid premature optimization
8. **Lock Minimization**: Minimize lock contention; use lock-free data structures when beneficial
9. **I/O Optimization**: Use asynchronous I/O and buffering for disk and network operations
10. **SIMD Utilization**: Use compiler auto-vectorization and explicit intrinsics where beneficial
11. **Memory Mapping**: Use memory-mapped files for efficient access to large files
12. **Performance Budgets**: Establish and monitor performance budgets for components

## Memory Management

1. **RAII Principle**: Tie resource lifetime to object lifetime
2. **Smart Pointers**: Prefer smart pointers over raw pointers
3. **Container Choice**: Use appropriate STL containers; prefer `std::vector` for most cases
4. **Allocation Awareness**: Be aware of allocation costs; minimize in performance-critical code
5. **Memory Pools**: Use object pools for frequently allocated/deallocated objects
6. **No Memory Leaks**: Use tools like Valgrind, AddressSanitizer to detect leaks
7. **Bounds Checking**: Use bounds-checked access in debug mode; consider release mode checks
8. **Large Allocations**: Handle large allocations gracefully; check for failure
9. **Stack vs Heap**: Prefer stack allocation for small, short-lived objects
10. **Move Semantics**: Use move constructors/assignment operators for efficient transfers
11. **Allocator Awareness**: Use allocators when custom memory behavior is needed
12. **Memory Fragmentation**: Be aware of fragmentation; consider memory layout

## Code Review Expectations

1. **Mandatory Reviews**: All changes to production code require review
2. **Review Scope**: Check correctness, clarity, completeness, and conformity to standards
3. **Constructive Feedback**: Provide specific, actionable feedback; focus on code not author
4. **Acceptance Criteria**: Verify that changes meet requirements and include tests
5. **Documentation Check**: Verify documentation is updated as needed
6. **Test Check**: Verify appropriate tests exist and pass
7. **Architecture Check**: Verify changes align with architectural vision
8. **Performance Check**: Consider performance implications
9. **Security Check**: Consider security implications
10. **Review Timeliness**: Perform reviews promptly to avoid blocking development
11. **Review Artifacts**: Use comments, discussions, and approval mechanisms in GitHub
12. **Knowledge Sharing**: Use reviews to share knowledge and spread understanding

## Secret Management

1. **Never in Code**: Never commit passwords, API keys, or credentials to repository
2. **Environment Variables**: Use environment variables for secrets when appropriate
3. **Secret Vaults**: Use secure vaults (HashiCorp Vault, AWS Secrets Manager, etc.) for production
4. **Configuration Files**: Never store secrets in plaintext configuration files
5. **Git Hooks**: Use pre-commit hooks to detect accidental secret commits
6. **Audit Regularly**: Regularly scan repository for exposed secrets
7. **Rotation Plan**: Have plan for rotating secrets when needed
8. **Least Privilege**: Grant minimum necessary permissions for secrets
9. **Logging**: Never log secrets or credentials
10. **Secrets in Logs**: Ensure logging frameworks don't accidentally capture secrets

## Large File Policy

1. **No Binary Blobs**: Avoid committing large binary files to repository
2. **Assets**: Store large assets (images, models, etc.) in asset management systems
3. **Test Data**: Keep test data small and symbolic; generate large test data on demand
4. **Documentation**: Reference large files stored elsewhere rather than committing them
5. **Git LFS**: Consider Git Large File Storage for unavoidable large binaries
6. **Artifact Storage**: Use artifact repositories for build outputs and dependencies
7. **Size Limits**: Implement repository hooks to reject files over certain size
8. **Binary Detection**: Avoid committing compiled binaries, object files, or libraries
9. **Progressive Download**: Design for progressive downloading of large resources
10. **CDN Usage**: Use content delivery networks for large static assets when applicable

## Commit Policy

1. **Compilable State**: Every commit must leave the project in a compilable state
2. **Test Passing State**: Every commit should not break existing tests
3. **Atomic Changes**: Each commit should be a single logical change
4. **No Work-in-Progress**: Avoid committing incomplete or broken features
5. **Revert Cleanly**: If reverting, revert the exact commit(s) that introduced the issue
6. **Amend with Care**: Use `git commit --amend` only for the most recent commit and only if not pushed
7. **Sign Off**: Consider using DCO (Developer Certificate of Origin) for contributions
8. **Branch Hygiene**: Delete feature branches after merging (both local and remote)
9. **Tagging**: Use tags for releases; annotated tags for official releases
10. **History Integrity**: Avoid rewriting public history; use force push only with extreme caution

## Checkpoint Policy

1. **Regular Checkpoints**: Create checkpoints at logical completion points
2. **Checkpoint Commits**: Use descriptive messages like `chkpt: completed ntfs parser implementation`
3. **Frequency**: Checkpoint at least daily or after significant progress
4. **Branch Checkpoints**: Consider checkpointing on feature branches before rebasing
5. **Documentation**: Update `docs/SESSION_STATE.md` with each checkpoint
6. **Revert Points**: Checkpoints should represent points you'd be comfortable reverting to
7. **Team Visibility**: Make checkpoints visible to team through branching strategy
8. **Automation Friendly**: Design checkpoints to be usable by automation/CI systems
9. **Not a Replacement**: Checkpoints do not replace proper testing and review processes
10. **Milestone Markers**: Use checkpoints to mark progress toward milestones

## Additional Rules

1. **Copyright Headers**: Use appropriate copyright and license headers on source files
2. **License Compliance**: Ensure all used libraries have compatible licenses
3. **Export Control**: Be aware of export control restrictions for cryptographic code
4. **Accessibility**: Consider accessibility in GUI design and documentation
5. **Internationalization**: Design for i18n/l10n where appropriate
6. **Maintenance Burden**: Consider long-term maintenance when accepting dependencies
7. **Deprecation Policy**: Mark deprecated features clearly; provide migration path
8. **Code Ownership**: While encouraging collective ownership, maintain clear responsibility
9. **Continuous Learning**: Stay updated with C++ best practices and language evolution
10. **Psychological Safety**: Foster environment where questions and concerns are welcomed
11. **Sustainable Pace**: Avoid heroics; maintain sustainable development pace
12. **Fun**: Remember to enjoy the process of building something great together

These rules will be periodically reviewed and updated as the project evolves. All contributors are expected to know and follow these rules.