# 5250ng Roadmap (Qt + C++)

## Key Engineering Decisions
1. **Protocol:** Implement TN5250 based on RFC 1205 and IBM documentation.
2. **Rendering Strategy:** Custom Qt `QWidget` with `QPainter` for full display control, including attributes, cursor, and 27×132 mode.
3. **Architecture:** Layered modular system (Core / Transport / Display / UI / AI) with firm interface separation and dependency inversion across boundaries.
4. **Threading Model:** TN5250 socket handling and parsing in a worker thread; GUI updates performed via Qt signals/slots only.
5. **AI Integration:** Optional plugin layer allowing multiple providers. Disabled by default and decoupled from core functionality.

---

## Testing Strategy

### Unit Tests
| Component | Coverage Target |
|----------|------------------|
| Protocol parser | Sequence parsing, state transitions |
| EBCDIC ↔ UTF-8 conversion | Full table correctness |
| Screen buffer | Cursor logic, protected/unprotected fields, scroll |
| Attribute rendering | Color, reverse video, blink, underline |
| Config and session objects | Load, save, modify, validation |

### Integration Tests
- Socket connection through handshake
- TLS and non-TLS negotiation
- Rendering updates triggered by protocol messages
- Keyboard input correctly transmitted to TN5250 server

### Cross-Platform Validation
- Linux (Ubuntu, Fedora)
- macOS (latest)
- Windows (10, 11)

### Performance Scenarios
- Rendering stress with large screens (27×132)
- High-frequency updates
- Simulated packet delay and disconnects
- Long-running memory observation

---

## Risks and Mitigation

| Risk | Mitigation Strategy |
|------|---------------------|
| TN5250 implementation complexity | Build unit tests early, study RFC and IBM ref examples, inspect open-source implementations |
| Incorrect rendering versus official emulators | Use reference screenshots and test on a real AS/400 |
| Platform-specific GUI/syscall issues | CI runs builds and UI tests on all OS starting Week 2 |
| AI scope creep delaying core features | Implement AI as plugin separated from main code, feature-flagged |

---

## Success Criteria
- Reliable TN5250 connection with TLS support.
- Rendering fidelity matching IBM reference (visually validated against ACS or commercial emulator).
- PF-keys, special keys, cursor navigation and field editing fully functional.
- Multi-session support with persistent configuration.
- Application packages delivered for Linux, macOS and Windows.
- Documentation and test coverage suitable for long-term maintenance.
- AI integration available but not required for base functionality.

---

## Timeline (Estimated)

| Phase | Weeks | Deliverables |
|-------|--------|--------------|
| 1 — Foundation | 1–2 | Repo structure, CMake + Qt6 skeleton, CI configured |
| 2 — Transport Layer | 3–6 | Socket client, TN5250 handshake, Telnet options, TLS |
| 3 — Display Engine | 7–10 | Screen buffer, render widget, cursor, attributes |
| 4 — Input Layer | 11–12 | PF-key mapping, keyboard → protocol encoding |
| 5 — UX and Sessions | 13–14 | Connect dialog, saved session profiles, logs |
| 6 — Optional AI Module | 15–18 | Provider abstraction, insights, on-display annotations |
| 7 — Polish and Release | 19–20 | Installers, documentation, performance optimization |

Total duration estimate: 20 weeks.

---

## Workflow Guidelines
1. Complete each phase only after tests from earlier phases are green.
2. Maintain documentation continuously, not only at end.
3. Tag repository milestones and keep a CHANGELOG.
4. Adjust roadmap only through documented change decisions.
5. Perform manual validation against a real AS/400 before final release.

---

## Getting Started
1. Prepare environment (Qt6 SDK, CMake, compiler toolchain).
2. Create directory structure and baseline CMake project.
3. Add CI pipelines and build targets for Linux, macOS, Windows.
4. Begin Phase 2 by writing first protocol parser tests before implementation.

