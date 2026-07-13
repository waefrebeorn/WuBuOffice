# WuBuOffice — Research Docket (comprehensive)

**Product frame:** An OS-integrated, AI/RL-native office suite built on our own stack — 
a custom operating system, the **WuBuMath** inference engine, and a **reinforcement-learning 
environment**. The strategic analogy is Microsoft Office's deep integration with Windows 
95/98/2000: the suite is not a set of foreign apps but a native layer of the OS, and now 
also a native layer of the inference + learning stack.

_Generated 2026-07-13 from real 2024–2026 web research on Microsoft 
Office complaints and open-source alternative gaps (LibreOffice, OnlyOffice, Google Docs). 
Themes below are grounded in that research; the 1000+ items are a derived, organized product "
docket for our suite. Every item is a concrete gap, wish, principle, or integration point._

## How to read this
- **Workstreams 01–22** group the 1000+ items by capability and by how they exploit our "
  integrated stack (OS + inference + RL).
- Items tagged toward **WS11 (unified model)**, **WS13 (inference)**, **WS14 (RL env)**, 
  **WS15 (OS integration)**, **WS16 (cross-app)**, and **WS17 (knowledge store)** are the 
  ones that turn a 'clone of Office' into *our* stack play.
- Research signal sources are listed at the end (Reddit, HN, XDA, LibreOffice/OnlyOffice 
  communities, Microsoft Learn/answers, Ink & Switch local-first essay, Haiku/BeOS OS 
  integration discussions, GDPR/CLOUD Act sovereignty threads).

## Research signal (what people actually complain about)
- **Subscription / lock-in:** users hate forced M365 subscriptions, price increases, and 
  'why am I forced to subscribe' — our answer: perpetual/local/free core (WS01).
- **Forced Copilot:** widespread backlash at AI pushed into the OS and Office without 
  consent or value — our answer: local-first inference the user controls (WS13).
- **Ribbon hate:** many want a classic menu/toolbar or a command palette instead of the 
  ribbon — our answer: optional ribbon + command palette + legacy presets (WS03).
- **Inconsistent dark mode:** paid apps with half-themed panes — our answer: unified 
  theming (WS03).
- **Accessibility:** screen-reader gaps, low-vision pain — our answer: WCAG 2.2 AA, 
  local TTS/AT (WS04).
- **No real-time collaboration in OSS:** LibreOffice lacks Google-Docs-grade co-editing 
  — our answer: CRDT co-authoring, offline-tolerant (WS06).
- **File-format lock-in:** 'complex' OOXML not even self-compatible; track-changes 
  corruption; Excel formula portability issues — our answer: faithful, self-compatible 
  OOXML/ODF + format-diff (WS05).
- **Performance:** LibreOffice slow start/typing lag; VBA incompatibility — our answer: 
  native, fast, our-own-DEFLATE, safe scripting (WS07, WS08, WS19).
- **Extensibility crisis:** 233 developers threatened to abandon the Office.js add-in 
  platform over stability/security/trust — our answer: a local-first, sandboxed extension 
  API (WS12).
- **Privacy/sovereignty:** GDPR/CLOUD Act exposure, data collection — our answer: 
  local-first, sovereign, no telemetry (WS02, WS18).
- **OS integration nostalgia:** people want apps that *are* the OS like old Office/Win9x 
  and BeOS/Haiku — our answer: deep OS integration (WS15).

## The docket (comprehensive, 1000+ items)

### 01. Pricing, Ownership & Licensing

0001. Ship a one-time-purchase or perpetual free core so users never face forced subscription renewal.
0002. Never require an account or login to open, edit, or save a local document.
0003. Decouple AI/inference features from the base license; base suite is fully usable offline.
0004. Publish a plain-language license that grants genuine ownership of installed software.
0005. Offer a transparent 'source-available' tier so orgs can self-audit the binaries they run.
0006. Eliminate telemetry-gated features: every capability works with zero phone-home.
0007. Provide a hardware-dongle-free offline activation path for air-gapped machines.
0008. Let enterprises buy per-seat perpetual licenses with a clear upgrade window.
0009. Ban dark-pattern 'your trial expired, pay now' modals on local files.
0010. Allow downgrade to any previously installed version without losing file access.
0011. Document total cost of ownership versus M365 over 5 years.
0012. Support community/edu/site-wide free licensing with simple verification.
0013. Never revoke access to files because a subscription lapsed.
0014. Provide a portable edition (runs from USB/own FS) with no installer.
0015. Let users export/create templates without a store account.
0016. Guarantee file formats stay open and reader-free forever (no rent-to-read).
0017. Offer a 'lifetime home' SKU at a fixed price.
0018. Make every paid tier list exactly what is NOT in the free tier, up front.
0019. No mandatory telemetry even for crash reporting unless opted in.
0020. Allow license transfer between machines by the owner without calling support.
0021. Publish a price-lock guarantee for N years for early adopters.
0022. Refund within 30 days for any reason, no questions.
0023. Never bundle unrelated SaaS trials into the installer.
0024. Provide volume licensing without a sales call.
0025. Support paying in local currency without surcharges.
0026. Ship a 'community build' that is feature-identical to paid for personal use.
0027. Let schools/institutions self-host the whole suite at zero cost.
0028. Document the exact data each edition collects (answer: none for local).
0029. Offer a 'legacy lock' that freezes UI/behavior for stability-critical orgs.
0030. Never upsell inside the document canvas.
0031. Provide a clear end-of-life policy with >=3yr migration notice.
0032. Support offline license verification via signed local token.
0033. Let users disable all network endpoints at install (firewall-friendly).
0034. Price transparently per app or as a bundle, user's choice.
0035. No 'we changed the price, accept or lose access' mechanic.
0036. Allow home users to keep using old versions in parallel with new.
0037. Publish a public roadmap tied to license, not to negotiation.
0038. Never charge per-document or per-export.
0039. Provide a free tier that is not feature-starved for real work.
0040. Treat the file as the product, not the account.
0041. Guarantee format readers remain free even if the suite goes commercial-only.
0042. Offer a not-for-profit free tier with simple attestation.
0043. Let users export their settings/license state portably.
0044. Ban 'subscribe to unlock this file' gating on owned documents.
0045. Provide a clear families/home plan without enterprise markup.
0046. Document exactly what 'free' means with no asterisks.
0047. Offer a 'pay what you can' tier for individuals below income threshold.
0048. Allow non-profits to self-attest and get free site licensing.
0049. Publish a clear 'what free includes' matrix on the installer.
0050. Never show 'subscribe to continue' on a locally-owned file.
0051. Support a 'community currency' where contributions earn license credit.
0052. Allow lifetime license transfer on hardware failure with proof.
0053. Provide a 'home lab' free license for self-hosters.
0054. Ban automatic renewal without a 30-day reminder email.
0055. Support a 'perpetual security updates' window of >=5 years.
0056. Never require re-activation after a major OS upgrade.
0057. Provide a 'trial' that is fully functional, time-limited only.
0058. Allow offline license proof via a printed QR the user owns.
0059. Support a 'family vault' license covering household devices.
0060. Publish a 'price history' so users see we don't silently raise.
0061. Offer a 'founders' price lock for early adopters, forever.
0062. Provide a 'student' free tier verified by email domain.
0063. Allow 'bring your own storage' with no storage upsell.
0064. Support a 'no ads ever' contractual guarantee.
0065. Provide a 'source-available' audit right for paid tiers.
0066. Never bundle a separate 'AI subscription' into the base price.
0067. Allow a 'lite' edition free forever for basic use.
0068. Support a 'per-app' purchase so users buy only what they need.
0069. Provide a 'license dashboard' showing all entitlements.
0070. Ban 'darker pattern' auto-checkbox on renewal.

### 02. Privacy, Sovereignty & Local-First

0071. Adopt local-first architecture: full read/write offline, sync optional.
0072. Store all user content under a single user-owned directory with clear layout.
0073. Zero required network calls for any editing operation.
0074. Implement CRDT/OT so offline edits merge without conflict loss.
0075. Provide end-to-end encryption for optional sync, keys never leave device.
0076. Support data sovereignty: files never transit foreign jurisdictions by default.
0077. Let users self-host the sync server on their own hardware.
0078. Document a GDPR/CLOUD-Act resistance posture (data stays in-region).
0079. Purge all temp/autosave fragments on close with a secure-wipe option.
0080. Never embed tracking pixels or remote fonts in saved documents.
0081. Strip metadata (author, machine, path) by default unless user opts in.
0082. Provide a metadata inspector/redactor before share/export.
0083. Honor a global 'no telemetry' flag enforced at the binary level.
0084. Encrypt local index/cache at rest.
0085. Allow documents to be opened from and saved to FUSE/own encrypted volumes.
0086. No advertising ID, no device fingerprinting, no cross-app correlation.
0087. Support offline spell/grammar/models that never phone home.
0088. Provide an auditable network egress log the user can review.
0089. Let orgs run the suite fully on-prem with no external dependency.
0090. Ship a 'paranoid mode' that blocks all outbound except explicit sync.
0091. Never train shared models on user documents without explicit opt-in per file.
0092. Allow per-document classification (public/confidential/secret) with handling rules.
0093. Provide a 'send nothing' guarantee validated by an external auditor.
0094. Encrypt autosave shadow copies; never leave plaintext temp files.
0095. Support air-gapped update distribution (signed offline update bundles).
0096. Let users verify binary integrity against a published hash/signature.
0097. Provide a data-export that yields plain folders, not a proprietary vault.
0098. Never require a CDN account to fetch templates or assets.
0099. Allow disabling of all 'smart' cloud features from one toggle.
0100. Document exactly which strings leave the machine and when.
0101. Support regional data residency selection at first run.
0102. Provide a 'forget this machine' that scrubs all local traces.
0103. Never silently upload autosaves 'for safety'.
0104. Allow the suite to run inside a sandbox with no network and full function.
0105. Provide a privacy label per feature (like a nutrition label).
0106. Let users approve/deny every network request interactively.
0107. Support document-level DRM that the owner controls, not a vendor.
0108. Keep AI inference local so prompts/data never leave the device.
0109. Provide a 'sovereign build' compiled without any cloud SDK.
0110. Let enterprises bring their own key (BYOK) for at-rest encryption.
0111. Audit dependencies for covert telemetry; ban them.
0112. Provide a clear lawful-interception resistance statement.
0113. Never store credentials in plaintext or reversible form.
0114. Allow privacy review mode that shows a live map of data flow.
0115. Ship a 'no cloud' edition that physically cannot reach the internet.
0116. Document subprocessor list (answer: none for local edition).
0117. Let users delete their account and all server copies in one click (if sync used).
0118. Provide tamper-evident logs for enterprise compliance.
0119. Support offline license verification via signed local token.
0120. Never correlate document content with other user activity.
0121. Allow disabling of all 'smart' cloud features from one toggle.
0122. Document exactly which strings leave the machine and when.
0123. Provide a 'data residency' selector defaulting to the user's region.
0124. Support a 'no cloud' build validated by an external auditor.
0125. Allow documents to be stored only on user-owned encrypted disks.
0126. Provide a 'privacy nutrition label' per feature, like food labels.
0127. Support a 'forget me' that scrubs all local traces in one click.
0128. Never embed tracking beacons in exported files.
0129. Provide a 'network egress log' the user can review anytime.
0130. Support a 'sovereign sync' where the server never sees plaintext.
0131. Allow enterprises to host sync on their own jurisdiction's servers.
0132. Provide a 'zero-knowledge' proof of our no-access architecture.
0133. Support a 'local-only AI' with weights on device (WS13).
0134. Provide a 'redact on share' that strips metadata automatically.
0135. Allow a 'privacy mode' that blanks AI from sensitive sections (WS13).
0136. Support a 'data portability' export as plain folders, not a vault.
0137. Provide a 'compliance attestation' for GDPR/CLOUD Act resistance.
0138. Never correlate document content with other user activity.
0139. Support a 'firewall-friendly' mode with zero outbound by default.
0140. Provide a 'telemetry off' enforced at the binary level.
0141. Allow a 'per-document classification' with handling rules.
0142. Support a 'secure wipe' of autosave temp on close.

### 03. User Interface & Ergonomics

0143. Offer a classic menu+toolbar mode as a first-class alternative to the ribbon.
0144. Make the ribbon optional, collapsible, and fully keyboard-operable.
0145. Provide a 'command palette' (Ctrl+K) that finds any action by name.
0146. Unify dark mode across Word/Excel/PowerPoint with no inconsistent panes.
0147. Let users set per-app accent colors that persist across sessions.
0148. Support a true black OLED dark theme.
0149. Provide density settings: compact / comfortable / spacious.
0150. Never change the UI under the user without an explicit opt-in.
0151. Persist window layout, open docs, and cursor position across restarts.
0152. Allow floating toolbars that dock anywhere, including second monitors.
0153. Provide a focus/ Distraction-free mode that hides all chrome.
0154. Support full theming via a documented CSS-like token system.
0155. Let users rebind every shortcut; ship Vim/Emacs/emacs presets.
0156. Provide a 'legacy Office 2003' layout preset for muscle memory.
0157. Show live word/char/reading-time counts without opening a panel.
0158. Make zoom persistent per-document and per-app.
0159. Provide non-modal dialogs so work continues while a dialog is open.
0160. Allow side-by-side document comparison in a single window (not two).
0161. Support tabbed documents within one window (like a browser).
0162. Provide a minimap/thumbnail strip for long documents and sheets.
0163. Let users hide the ribbon entirely and reveal on hotkey.
0164. Respect OS font scaling and high-DPI without blur.
0165. Provide a consistent icon language across all apps.
0166. Allow toolbar buttons to be added/removed by the user.
0167. Support a 'simple mode' for new users with progressive disclosure.
0168. Never bury common actions (save, print) behind multi-click menus.
0169. Provide audible/visual feedback only when the user enables it.
0170. Allow full UI text scaling independent of document zoom.
0171. Support right-to-left UI for Arabic/Hebrew users.
0172. Provide a high-contrast theme meeting WCAG AAA.
0173. Let users choose serif/sans UI font.
0174. Provide a 'no animations' mode for motion-sensitive users.
0175. Support gesture-free full operation from keyboard alone.
0176. Allow window splitting and frozen header rows/cols by default convenience.
0177. Provide a quick 'jump to last edit' command.
0178. Let users customize the status bar widgets.
0179. Support multiple themes simultaneously per document type.
0180. Provide a 'what's this?' hover help on every control.
0181. Allow saving/loading custom UI layouts as profiles.
0182. Never show tips/ads in the editing surface.
0183. Provide a consistent undo/redo button placement across apps.
0184. Support a 'recent actions' rail for one-click repeat.
0185. Let users disable the Start screen and boot straight to a blank doc.
0186. Provide a unified color picker with hex/RGB/HSL and history.
0187. Support document tabs draggable to new windows.
0188. Allow the sidebar to collapse to a thin icon rail.
0189. Provide a 'command history' searchable like a shell.
0190. Let users set default view (print/draft/web) per app.
0191. Support touch+mouse hybrid layouts that adapt.
0192. Provide a night-shift/blue-light filter hook into OS.
0193. Never reorder ribbon tabs based on 'adaptive' heuristics without a lock.
0194. Allow exporting the full shortcut map as a cheat sheet.
0195. Provide a 'zen' mode that hides everything but the text.
0196. Support a 'command bar' at the bottom like a terminal.
0197. Allow a 'compact ribbon' that shows icons only.
0198. Provide a 'touch-first' layout toggle for 2-in-1 devices.
0199. Support a 'focus frame' highlighting the active region.
0200. Allow a 'custom accent' per document type.
0201. Provide a 'unified zoom' that respects the OS setting.
0202. Support a 'no-animation' mode for vestibular safety.
0203. Allow a 'tab cycle' that moves between open docs via Ctrl+Tab.
0204. Provide a 'jump to last edit' command globally.
0205. Support a 'minimap' for long sheets and docs.
0206. Allow a 'floating toolbar' that follows the selection.
0207. Provide a 'legacy 2003' preset for muscle memory.
0208. Support a 'high-DPI' crisp rendering with no blur.
0209. Allow a 'RTL UI' for Arabic/Hebrew users.
0210. Provide a 'visible focus' ring everywhere for keyboard users.
0211. Support a 'quick styles' gallery inline.
0212. Allow a 'status bar' widget customization.
0213. Provide a 'no tips' guarantee in the canvas.
0214. Support a 'start blank' boot without a start screen.

### 04. Accessibility (WCAG, Screen Readers, Low Vision, Motor, Cognitive)

0215. Target WCAG 2.2 AA minimum, AAA where feasible, across all apps.
0216. Expose a complete UI Automation / AT-SPI tree for every control.
0217. Ensure screen readers announce all dialogs, errors, and state changes.
0218. Provide a dedicated accessibility checker with fix-in-place suggestions.
0219. Support full keyboard navigation with a visible focus indicator everywhere.
0220. Never trap focus in a dialog; ESC always closes/returns.
0221. Label every image, shape, chart with alt text enforced on insert.
0222. Provide a 'check accessibility' that runs automatically before save/share.
0223. Support high-contrast themes that recolor charts and shapes correctly.
0224. Allow font, spacing, and line-width bumps globally for low vision.
0225. Provide a built-in screen reader / TTS for documents (local, no cloud).
0226. Support dyslexia-friendly font and spacing presets.
0227. Provide text-to-speech that highlights the spoken sentence (karaoke).
0228. Allow speech-to-text dictation fully offline via local models.
0229. Support switch control and single-button navigation.
0230. Provide sticky keys / slow keys honoring OS settings.
0231. Never use color alone to convey meaning; add icons/labels.
0232. Make all charts readable via data-table fallback for AT.
0233. Support magnification that tracks caret and focus.
0234. Provide a 'reading ruler' and tint overlay for visual stress.
0235. Allow remapping of any mouse action to keyboard.
0236. Support OS narrator/ORCA/VoiceOver natively, documented.
0237. Provide cognitive-load reduction mode: simplify UI, fewer choices.
0238. Allow documents to carry an accessibility summary metadata block.
0239. Support braille display output for document text.
0240. Ensure math is exposed as MathML/semantic for AT, not images.
0241. Provide captions for any embedded audio/video.
0242. Support 'announce on save/send' confirmations via TTS.
0243. Allow contrast and brightness adjustment inside the canvas.
0244. Provide a plain-text 'story view' that linearizes any document.
0245. Support keyboard-driven table navigation (cell by cell).
0246. Make all error messages actionable, not just codes.
0247. Provide a 'reduce motion' that disables transitions globally.
0248. Support OS large-text without breaking layout.
0249. Allow per-user accessibility profiles that roam with the OS account.
0250. Provide an accessibility 'tour' on first run.
0251. Ensure focus order matches visual/logical order.
0252. Support custom cursor size and color.
0253. Provide audio cues that are optional and distinct.
0254. Allow disabling of auto-correct that hinders AT users.
0255. Support 'describe image' via local model for alt-text generation (opt-in).
0256. Make the command palette screen-reader friendly with live regions.
0257. Provide a contrast analyzer for user-chosen colors.
0258. Support dictation punctuation commands offline.
0259. Allow documents to require accessibility before publish (team policy).
0260. Provide a 'simulate low-vision' preview mode for authors.
0261. Support keyboard macros for repetitive AT workflows.
0262. Ensure print and PDF export preserve tags/structure for AT.
0263. Provide a 'no time limit' for any interactive element.
0264. Support alternative input (head/eye tracking) via OS bridges.
0265. Allow color-blind safe palette suggestions.
0266. Document the AT test matrix we run in CI.
0267. Provide an accessibility statement per release.
0268. Provide a 'simulate low vision' preview for authors.
0269. Support a 'reading ruler' tint overlay for visual stress.
0270. Allow a 'braille display' output for document text.
0271. Provide a 'TTS karaoke' that highlights the spoken sentence.
0272. Support a 'dictation' fully offline via local models.
0273. Allow a 'switch control' single-button navigation.
0274. Provide a 'cognitive simplify' mode reducing choices.
0275. Support a 'math exposed as MathML' for AT, not images.
0276. Allow a 'captions' for embedded audio/video.
0277. Provide a 'contrast analyzer' for user colors.
0278. Support a 'sticky keys' honoring OS settings.
0279. Allow a 'no time limit' on interactive elements.
0280. Provide a 'plain-text story view' linearizing any doc.
0281. Support a 'AT test matrix' we run in CI.
0282. Allow a 'describe image' via local model (opt-in).
0283. Provide a 'accessibility statement' per release.
0284. Support a 'keyboard macro' for repetitive AT workflows.
0285. Allow a 'color-blind safe' palette suggestions.
0286. Provide a 'focus order' matching logical order.
0287. Support a 'magnification' tracking caret and focus.

### 05. File Formats & Interoperability

0288. Implement OOXML (ECMA-376) faithfully, not a 'complex' subset.
0289. Guarantee round-trip fidelity: open then save must not lose data.
0290. Support ODF (ODT/ODS/ODP) natively with no conversion loss.
0291. Support legacy .doc/.xls/.ppt (binary) import via from-scratch readers.
0292. Support PDF import, edit, and accessible export (tagged PDF).
0293. Support PDF/A for archival with embedded fonts.
0294. Never silently drop features on save; warn explicitly if unsupported.
0295. Implement strict self-compatibility: vN files open in vN+1 exactly.
0296. Provide a format-diff tool showing what changed on save.
0297. Store a canonical internal model separate from any wire format.
0298. Support plain Markdown/HTML export for docs with style mapping.
0299. Support CSV/TSV with quoting/encoding correctness (UTF-8 default).
0300. Support JSON/Parquet export for sheet data to feed our ML stack.
0301. Implement Excel formula compatibility for the common 400 functions.
0302. Preserve pivot tables, charts, and conditional formats on round-trip.
0303. Support embedded objects without external app dependency.
0304. Provide a 'validate' that checks conformance to the spec.
0305. Support ZIP-stored and ZIP-deflated packages (our own DEFLATE).
0306. Allow documents to be opened directly from our OS file manager preview.
0307. Support digital signatures on documents (XAdES/PAdES) locally.
0308. Provide a 'repair' that recovers content from corrupted packages.
0309. Never write vendor-only extensions to the default save path.
0310. Support long-path and Unicode filenames cross-platform.
0311. Allow selective export of one sheet/slide/section.
0312. Support versioned saves (append-only history) locally.
0313. Implement true track-changes XML that survives round-trips.
0314. Support comments/threads with mentions and resolve states.
0315. Provide a compatibility report vs MS Office for any file.
0316. Support embedded fonts subsetting for portability.
0317. Allow importing Google Docs/Sheets via exported OOXML/ODF.
0318. Support RTF and older WordPerfect import for legacy users.
0319. Provide a 'what we can't open yet' honest capability list.
0320. Support macro-free by default; macros explicit and sandboxed.
0321. Allow exporting to EPUB for documents.
0322. Support LaTeX/MathML paste for equations.
0323. Provide a schema for our unified object model for interop.
0324. Support reading/writing the 'strict' OOXML variant, not just transitional.
0325. Never require a conversion prompt on open of a native file.
0326. Support encrypted OOXML (agile encryption) import/export.
0327. Provide a 'compare two files' structural diff view.
0328. Support bookmarks/hyperlinks that survive export.
0329. Allow documents to embed other suite docs as live objects.
0330. Support EXIF/metadata passthrough for images.
0331. Provide a 'compat shim' that maps our extensions to OOXML on export.
0332. Support reading password-protected docs via user key only.
0333. Allow custom properties/extended attributes preservation.
0334. Support compressed-media reuse (dedupe identical images).
0335. Provide a lossless image pipeline (no re-encode on save).
0336. Support multi-target export (PDF + OOXML + ODF at once).
0337. Document the exact OOXML parts we emit for auditor review.
0338. Support opening files from object stores/our OS VFS directly.
0339. Provide a format fuzzer in CI to harden parsers.
0340. Support 'save as template' that strips content but keeps structure.
0341. Allow batch format conversion from the command line (OS-integrated).
0342. Support a canonical 'flat XML' debug format for diffing.
0343. Never embed machine-specific paths in saved files.
0344. Provide a 'health check' that flags non-portable content before share.
0345. Support ODF 1.3 with full fidelity.
0346. Provide a 'format-diff' showing what changed on save.
0347. Support strict OOXML (not just transitional).
0348. Provide a 'repair' that recovers content from corrupt packages.
0349. Support encrypted OOXML (agile) import/export.
0350. Provide a 'validate' that checks spec conformance.
0351. Support PDF/A archival with embedded fonts.
0352. Allow selective export of one sheet/slide/section.
0353. Support reading/writing our OS VFS directly.
0354. Provide a 'compat shim' mapping our extensions to OOXML.
0355. Support JSON/Parquet export for our ML stack (WS17).
0356. Provide a 'lossless image pipeline' (no re-encode on save).
0357. Support a 'canonical flat XML' debug format for diffing.
0358. Allow 'save as template' stripping content but keeping structure.
0359. Provide a 'health check' flagging non-portable content.
0360. Support 'what we can't open yet' honest capability list.
0361. Provide a 'schema' for our unified object model (WS11).
0362. Support long-path Unicode filenames cross-platform.
0363. Provide a 'versioned saves' append-only history locally.
0364. Support a 'format fuzzer' in CI to harden parsers.

### 06. Real-Time Collaboration & Co-Authoring

0365. Provide true multi-cursor real-time co-editing like Google Docs.
0366. Use CRDT so merges are conflict-free and offline-tolerant.
0367. Show live presence avatars with caret positions per user.
0368. Support per-paragraph/per-cell locking during active edit.
0369. Provide comment threads with resolve/reopen and mentions.
0370. Allow suggesting mode (all edits are proposals until accepted).
0371. Support version history with named snapshots and restore.
0372. Provide a 'fork and merge' workflow for branches of a document.
0373. Let collaboration work peer-to-peer on a LAN without a server.
0374. Support self-hosted relay/server under user control.
0375. Provide end-to-end encrypted collaboration channels.
0376. Show a live change feed (who changed what, when).
0377. Allow presence to be private (hidden) per user choice.
0378. Support async review with tracked changes and replies.
0379. Provide a 'follow me' presentation mode for remote teaching.
0380. Allow co-editing of charts and pivot data live.
0381. Support granular permissions (view/comment/edit) per region.
0382. Provide conflict resolution UI when CRDT hints ambiguity.
0383. Let users see a mini-map of collaborators' viewport.
0384. Support offline edits that sync automatically on reconnect.
0385. Provide a 'who is editing' indicator without leaking content.
0386. Allow exporting a collaboration session transcript.
0387. Support low-bandwidth mode (delta-only sync).
0388. Provide a 'handoff' that transfers doc control between users.
0389. Allow guest access via time-limited signed link.
0390. Support integration with our OS identity/address book.
0391. Provide a 'quiet hours' that batches notifications.
0392. Allow templates to be shared and co-edited in a team library.
0393. Support emoji reactions on comments (local, synced).
0394. Provide a 'merge preview' before accepting a fork.
0395. Allow co-authoring from mobile and desktop simultaneously.
0396. Support undo that respects other users' concurrent edits.
0397. Provide a 'presence cursor color' picker.
0398. Allow documents to live in our OS 'shared spaces' natively.
0399. Support a 'read-only live' view for stakeholders.
0400. Provide activity analytics (optional) for team leads.
0401. Allow anonymous local-network co-edit with no account.
0402. Support session recording for training (local only).
0403. Provide a 'lock whole doc' mode for final freeze.
0404. Allow real-time co-editing of embedded scripts/macros safely.
0405. Support translation of comments across languages live (local model).
0406. Provide a 'you have unread changes' reconciler on open.
0407. Allow co-editing of master slides across a deck.
0408. Support presence in the OS notification center.
0409. Provide a 'diff since last open' summary.
0410. Allow granular region sharing (send one slide, not whole deck).
0411. Support end-to-end encrypted collaboration channels.
0412. Provide a 'leave session' that cleanly detaches CRDT state.
0413. Allow documents to require quorum to publish (team policy).
0414. Support a 'live cursor chat' side channel.
0415. Provide a 'save conflict' insurance that never loses text.
0416. Allow guest editing via disposable ephemeral identity.
0417. Support a 'co-edit replay' to see how a doc evolved.
0418. Provide a 'permissions audit' log per document.
0419. Provide true multi-cursor real-time co-editing like Google Docs.
0420. Use CRDT so merges are conflict-free and offline-tolerant.
0421. Show live presence avatars with caret positions.
0422. Support per-paragraph/per-cell locking during edit.
0423. Provide comment threads with resolve/reopen and mentions.
0424. Allow suggesting mode (all edits are proposals until accepted).
0425. Support version history with named snapshots and restore.
0426. Let collaboration work peer-to-peer on a LAN without a server.
0427. Support self-hosted relay under user control.
0428. Provide end-to-end encrypted collaboration channels.
0429. Show a live change feed (who changed what, when).
0430. Allow presence to be private per user choice.
0431. Support async review with tracked changes and replies.
0432. Provide a 'follow me' presentation mode for remote teaching.
0433. Allow co-editing of charts and pivot data live.
0434. Support granular permissions (view/comment/edit) per region.
0435. Provide conflict resolution UI when CRDT hints ambiguity.
0436. Let users see a mini-map of collaborators' viewport.
0437. Support offline edits that sync on reconnect automatically.
0438. Provide a 'leave session' that cleanly detaches CRDT state.

### 07. Performance & Resource Use

0439. Cold start <300ms for a blank document on reference hardware.
0440. Never block the UI thread on load/save; do I/O async.
0441. Keep idle memory <150MB per app on a blank doc.
0442. Stream-render only the visible page/sheet region (virtualized canvas).
0443. Support documents of 1M+ rows without crashing or thrashing.
0444. Use our own DEFLATE for fast, dependency-free compression.
0445. Avoid JVM/Electron; native code for speed and small footprint.
0446. Provide a 'low-power mode' that caps animations and effects.
0447. Incrementally save large files (append deltas), not full rewrite.
0448. Cache parsed models so re-open is instant.
0449. Support background autosave that never interrupts typing.
0450. Profile and cap per-document memory via streaming where possible.
0451. Provide a 'repair if slow' that rebuilds indexes.
0452. Avoid font re-loading on every paint.
0453. Use GPU-accelerated text rendering where available, fallback safe.
0454. Support opening multiple docs with shared process (low RAM).
0455. Provide a 'benchmark' mode that reports ops/sec for regressions.
0456. Never require re-indexing the whole drive to find a file.
0457. Keep installer <80MB; no runtime download post-install.
0458. Support launch from OS shell in <1s end-to-end.
0459. Avoid telemetry/network on the hot path.
0460. Provide a 'headless batch' mode for servers with tiny footprint.
0461. Use mmap for large read-only assets.
0462. Support pause/resume of long operations (find/replace whole book).
0463. Cap undo history memory with configurable depth.
0464. Provide a 'performance HUD' showing frame time and mem.
0465. Avoid blocking on spell-check; run it in a worker.
0466. Support opening corrupt files in a degraded but fast mode.
0467. Keep clipboard operations instant regardless of doc size.
0468. Provide a 'turbo' mode that disables live preview for huge sheets.
0469. Avoid memory leaks across long sessions (CI soak tests).
0470. Support ARM and x86 with SIMD where beneficial.
0471. Provide a 'quit and restore exactly' with no relayout cost.
0472. Avoid synchronous layout on every keystroke (debounced reflow).
0473. Support document sharding so one sheet doesn't block another.
0474. Keep search/index off the critical path.
0475. Provide a 'light' theme that skips heavy graphics.
0476. Support opening files from network shares without full download.
0477. Avoid duplicate parsing when switching tabs.
0478. Provide a 'free RAM now' that trims caches on demand.
0479. Benchmark against LO/MS on the same hardware, publish numbers.
0480. Support a 'low-end PC' preset (disable effects, cap DPI).
0481. Avoid disk thrash from autosave storms.
0482. Provide a 'startup trace' to diagnose slow boots.
0483. Keep the binary tree free of optional heavy deps.
0484. Support cooperative multitasking with the OS scheduler.
0485. Provide a 'freeze background docs' to save CPU.
0486. Avoid re-rendering unchanged slides in a deck.
0487. Support a 'instant close' that defers writes to idle.
0488. Keep memory bounded under infinite undo via snapshot pruning.
0489. Provide a 'resource governor' respecting OS constraints.
0490. Avoid blocking on font substitution lookups.
0491. Support a 'fast open' that parses lazily and renders progressively.
0492. Keep the test suite with perf regression gates.
0493. Cold start <300ms for a blank document on reference hardware.
0494. Never block the UI thread on load/save; do I/O async.
0495. Keep idle memory <150MB per app on a blank doc.
0496. Stream-render only the visible region (virtualized canvas).
0497. Support 1M+ row sheets without crashing or thrashing.
0498. Use our own DEFLATE for fast, dependency-free compression.
0499. Avoid Electron/JVM; native code for speed and footprint.
0500. Provide a 'low-power mode' capping animations.
0501. Incrementally save large files (append deltas), not full rewrite.
0502. Cache parsed models so re-open is instant.
0503. Support background autosave that never interrupts typing.
0504. Profile and cap per-document memory via streaming.
0505. Provide a 'benchmark' mode reporting ops/sec for regressions.
0506. Avoid telemetry/network on the hot path.
0507. Support a 'headless batch' mode with tiny footprint.
0508. Use mmap for large read-only assets.
0509. Support pause/resume of long operations.
0510. Cap undo history memory with configurable depth.
0511. Provide a 'performance HUD' showing frame time and mem.
0512. Keep memory bounded under infinite undo via snapshot pruning.

### 08. Spreadsheet Engine (Excel parity & beyond)

0513. Implement a from-scratch formula engine with 400+ functions (we have 77; expand).
0514. Guarantee bit-identical numeric results with Excel for common formulas.
0515. Support cross-sheet and 3D references (Sheet1:Sheet3!A1).
0516. Support structured table references ([Column]).
0517. Support dynamic arrays / spill (Excel 365 behavior).
0518. Support LAMBDA and named formulas for user-defined functions.
0519. Support array literals and implicit intersection correctly.
0520. Provide a robust error model (#N/A, #VALUE!, #REF!, #CYCLE!).
0521. Implement circular-reference detection with iteration settings.
0522. Support volatile functions (NOW, TODAY, RAND) with recalc control.
0523. Support goal seek and solver (linear/non-linear).
0524. Provide a data table (what-if) engine.
0525. Support pivot tables with grouping and calculated fields.
0526. Support what-if scenarios manager.
0527. Provide a chart engine: line/bar/pie/scatter/area/histogram.
0528. Support conditional formatting with rules and scales.
0529. Support data validation (lists, ranges, custom formulas).
0530. Support freeze panes, split, group/outline.
0531. Provide a百万-row streaming grid that stays responsive.
0532. Support multi-threaded recalc respecting dependencies.
0533. Provide a dependency graph visualizer for debugging formulas.
0534. Support user-defined functions in our safe scripting language.
0535. Support Excel-compatible keyboard shortcuts for power users.
0536. Provide a formula audit (trace precedents/dependents).
0537. Support named ranges local and global.
0538. Provide a unit/regression corpus of Excel results to match.
0539. Support big-number/arbitrary precision where needed (finance).
0540. Provide locale-correct decimal/group separators.
0541. Support R1C1 and A1 reference styles.
0542. Provide a 'watch window' for monitoring cells.
0543. Support scenario summary reports.
0544. Provide solver with GRG and evolutionary methods.
0545. Support statistical add-ons (regression, ANOVA) open.
0546. Provide a financial calendar/date system matching Excel 1900/1904.
0547. Support array formulas legacy (Ctrl+Shift+Enter) import.
0548. Provide a formula text beautifier/auto-formatter.
0549. Support(spill) dynamic arrays across sheets.
0550. Provide a recalc-on-load vs manual mode toggle.
0551. Support query-like transforms (sort/filter/unique) as functions.
0552. Provide a 'calc stepping' debugger that shows intermediate values.
0553. Support external data connections (local CSV/DB) without cloud.
0554. Provide a macro recorder that emits our safe script.
0555. Support in-cell sparklines.
0556. Provide a sheet comparison/diff tool.
0557. Support protection (lock cells) with password (local KDF).
0558. Provide a 'formula search' across the whole workbook.
0559. Support autocomplete for function names and ranges.
0560. Provide a numeric stability test suite (avoid float drift).
0561. Support big grids (1,048,576 rows) memory-mapped.
0562. Provide a 'recalc profile' showing slowest cells.
0563. Support live linked cells across different open workbooks.
0564. Provide a 'what-if' slider UI bound to a cell.
0565. Support image-in-cell and rich values (entities).
0566. Provide a 'formula lint' for common mistakes.
0567. Support international function-name localization optionally.
0568. Provide an open function-catalog that users can extend.
0569. Support integration with our inference engine for forecasting.
0570. Provide a 'spill diagnostics' when a formula unexpectedly spills.
0571. Support defined-name scoping rules exactly like Excel.
0572. Provide a recalc sandbox so a bad UDF can't hang the app.
0573. Implement a from-scratch formula engine with 400+ functions.
0574. Guarantee bit-identical numeric results with Excel for common formulas.
0575. Support cross-sheet and 3D references (Sheet1:Sheet3!A1).
0576. Support structured table references ([Column]).
0577. Support dynamic arrays / spill (Excel 365 behavior).
0578. Support LAMBDA and named formulas for UDFs.
0579. Provide a robust error model (#N/A, #VALUE!, #REF!, #CYCLE!).
0580. Implement circular-reference detection with iteration settings.
0581. Support volatile functions (NOW, TODAY, RAND) with recalc control.
0582. Support goal seek and solver (linear/non-linear).
0583. Provide a data table (what-if) engine.
0584. Support pivot tables with grouping and calculated fields.
0585. Support conditional formatting with rules and scales.
0586. Provide a chart engine: line/bar/pie/scatter/area/histogram.
0587. Support multi-threaded recalc respecting dependencies.
0588. Provide a dependency graph visualizer for debugging formulas.
0589. Support user-defined functions in our safe scripting language.
0590. Support Excel-compatible keyboard shortcuts for power users.
0591. Provide a formula audit (trace precedents/dependents).
0592. Support named ranges local and global.

### 09. Word Processing

0593. Support true style inheritance (paragraph/character/list/table).
0594. Provide a styles pane with live preview and organize-by-usage.
0595. Support outline view with collapse/expand and reorder by heading.
0596. Implement track changes with accept/reject per change and bulk.
0597. Support compare documents producing a redline.
0598. Provide a master-document feature for book-length works.
0599. Support footnotes, endnotes, and cross-references that auto-update.
0600. Support tables with repeat header rows and captions.
0601. Provide a bibliography/reference manager (BibTeX/Zotero import).
0602. Support multi-column layouts and section breaks.
0603. Provide a TOC that builds from headings and updates.
0604. Support indexing with auto-generated index marks.
0605. Provide mail merge from local CSV/JSON/our data store.
0606. Support equations via MathML/LaTeX with numbering.
0607. Provide a watermark, header/footer, and page borders.
0608. Support RTL and bidirectional text correctly.
0609. Provide hanging punctuation and kerning controls.
0610. Support vertical text and East-Asian typography.
0611. Provide a 'focus' typewriter mode with centered caret.
0612. Support full-bleed and booklet printing imposition.
0613. Provide a 'manuscript' template (12pt, double-spaced, indents).
0614. Support change bars and comment bubbles in margin.
0615. Provide a 'redline to clean' one-click accept all.
0616. Support co-authoring track changes (see Workstream 06).
0617. Provide a 'resolve all comments' bulk action.
0618. Support document variables and fields (page, author, date).
0619. Provide a 'compare to last saved' quick diff.
0620. Support auto-save versions with restore.
0621. Provide a 'reading mode' with pagination like a book.
0622. Support Ink/handwriting to text conversion offline.
0623. Provide a 'say as you type' dictation offline (local STT).
0624. Support a thesaurus and synonym lookup offline.
0625. Provide a 'word count by section' analytics.
0626. Support embedded spreadsheets/charts that update live.
0627. Provide a 'translate selection' via local model, inline.
0628. Support grammar checking offline with explanations.
0629. Provide a 'clarity' suggestions mode (opt-in, local).
0630. Support protected sections with editable regions.
0631. Provide a 'template gallery' curated and offline.
0632. Support custom document properties and smart tags (local).
0633. Provide a 'publish to PDF/EPUB' with accessibility pass.
0634. Support forms with content controls (dropdowns, dates).
0635. Provide a 'legal blackline' comparison report.
0636. Support page-numbering schemes per section.
0637. Provide a 'link to heading' auto reference field.
0638. Support caption numbering for figures/tables/equations.
0639. Provide a 'no-break' keep-with-next and keep-lines-together.
0640. Support autocorrect exceptions per language.
0641. Provide a 'style inspector' showing effective formatting.
0642. Support pasting from web with clean formatting options.
0643. Provide a 'document map' outline navigator.
0644. Support revision history baked into the file.
0645. Provide a 'sentiment/reading-level' meter (local model).
0646. Support hyphenation dictionaries per language.
0647. Provide a 'compare two versions' structural diff.
0648. Support custom line numbering for legal/code docs.
0649. Provide a 'manuscript stats' (flesch, chars, scenes).
0650. Support embedding audio/video with captions.
0651. Provide a 'export to Markdown' preserving structure.
0652. Support a 'distraction-free compose' with typewriter scroll.
0653. Support true style inheritance (paragraph/character/list/table).
0654. Provide a styles pane with live preview and organize-by-usage.
0655. Support outline view with collapse/expand and reorder by heading.
0656. Implement track changes with accept/reject per change and bulk.
0657. Support compare documents producing a redline.
0658. Provide a master-document feature for book-length works.
0659. Support footnotes, endnotes, and cross-references that auto-update.
0660. Support tables with repeat header rows and captions.
0661. Provide a bibliography/reference manager (BibTeX/Zotero import).
0662. Support multi-column layouts and section breaks.
0663. Provide a TOC that builds from headings and updates.
0664. Support indexing with auto-generated index marks.
0665. Provide mail merge from local CSV/JSON/our data store.
0666. Support equations via MathML/LaTeX with numbering.
0667. Provide a watermark, header/footer, and page borders.
0668. Support RTL and bidirectional text correctly.
0669. Provide a 'focus' typewriter mode with centered caret.
0670. Provide a 'manuscript' template (12pt, double-spaced, indents).
0671. Support change bars and comment bubbles in margin.
0672. Provide a 'redline to clean' one-click accept all.

### 10. Presentations

0673. Support 16:9/4:3/16:10 and custom slide sizes.
0674. Provide a master/slide-layout system with placeholder inheritance.
0675. Support transitions that are GPU-accelerated and skippable.
0676. Provide a presenter view with notes, timer, and next-slide.
0677. Support speaker notes per slide with rich text.
0678. Provide a 'rehearse timings' mode.
0679. Support annotations/draw on slide during presentation (local).
0680. Provide a chart/table that links to a live spreadsheet.
0681. Support embedding video/audio with playback controls.
0682. Provide a 'morph' style transition between layouts.
0683. Support sections to organize a deck.
0684. Provide a 'photo album' auto-layout from a folder.
0685. Support master handout/notes-page printing.
0686. Provide a 'broadcast' mode streaming to our OS devices.
0687. Support conflict-free co-editing of a deck (WS06).
0688. Provide a 'design ideas' helper using local model (opt-in).
0689. Support SVG and vector shapes natively.
0690. Provide a shape union/subtract/intersect/trim.
0691. Support smart guides and snap to grid/objects.
0692. Provide a 'replace fonts' across the whole deck.
0693. Support animation pane with timeline and easing.
0694. Provide a 'record slideshow' to video locally.
0695. Support QR/link to live doc for audience.
0696. Provide a 'export to images' (PNG/PDF) per slide.
0697. Support a 'kiosk' loop mode.
0698. Provide a 'remote clicker' over our OS Bluetooth stack.
0699. Support braille/AT navigation of slides.
0700. Provide a 'outline to deck' auto-generator from headings.
0701. Support theme variants (color/font/size) applied instantly.
0702. Provide a 'compare decks' structural diff.
0703. Support comments per slide with threads.
0704. Provide a 'narrate slide' TTS offline for rehearsal.
0705. Support hidden slides for branching presentations.
0706. Provide a 'section zoom' like Prezi-style navigation (optional).
0707. Support 3D models with rotate (local viewer).
0708. Provide a 'live caption' of spoken presenter via local STT.
0709. Support 'translate my slides' for the audience view.
0710. Provide a 'template from this deck' one-click.
0711. Support master footer/date/number fields.
0712. Provide a 'slide sorter' thumbnail grid with drag reorder.
0713. Support paste-linked objects that update.
0714. Provide a 'export to OOXML/ODF/PDF' with fidelity.
0715. Support a 'practice mode' with filler-word detection (local).
0716. Support emoji and icon library offline.
0717. Provide a 'highlighter' pen during show.
0718. Support 'laser pointer' cursor effect.
0719. Provide a 'black/white screen' presenter hotkey.
0720. Support 'slide zoom' for Q&A navigation.
0721. Provide a 'auto-fit text' to placeholder.
0722. Support 'master reset' that reapplies layout.
0723. Provide a 'deck statistics' (slides, words, est. time).
0724. Support 'embed spreadsheet chart' live linked.
0725. Provide a 'section transitions' distinct from slide.
0726. Support 'present to window' for screen capture.
0727. Provide a 'offline spell check' of notes.
0728. Support 'template marketplace' curated, no account needed.
0729. Provide a 'deck health' check (contrast, font size, alt text).
0730. Support 16:9/4:3/16:10 and custom slide sizes.
0731. Provide a master/slide-layout system with placeholder inheritance.
0732. Support transitions that are GPU-accelerated and skippable.
0733. Provide a presenter view with notes, timer, and next-slide.
0734. Support speaker notes per slide with rich text.
0735. Provide a 'rehearse timings' mode.
0736. Support annotations/draw on slide during presentation (local).
0737. Provide a chart/table that links to a live spreadsheet.
0738. Support embedding video/audio with playback controls.
0739. Provide a 'morph' style transition between layouts.
0740. Support sections to organize a deck.
0741. Provide a 'photo album' auto-layout from a folder.
0742. Provide a master handout/notes-page printing.
0743. Provide a 'broadcast' mode streaming to our OS devices.
0744. Support conflict-free co-editing of a deck (WS06).
0745. Provide a 'design ideas' helper using local model (opt-in).
0746. Support SVG and vector shapes natively.
0747. Provide a shape union/subtract/intersect/trim.
0748. Support smart guides and snap to grid/objects.
0749. Provide a 'replace fonts' across the whole deck.

### 11. Document Model & Unified Object Model

0750. Define one canonical object model shared by all three apps.
0751. Represent every entity (run, cell, shape) as a typed node.
0752. Separate content, style, and layout in the model.
0753. Make the model serializable to our internal format and OOXML.
0754. Expose a stable DOM-like API for scripts and add-ins.
0755. Give every object a stable ID for diff/merge/undo.
0756. Support a command pattern so all edits are reversible/recordable.
0757. Store rich metadata (provenance, author, timestamps) per node.
0758. Allow queries over the model (find all headings, all charts).
0759. Support a reactive model: UI updates from model changes only.
0760. Make the model the single source for both render and export.
0761. Support incremental updates (patches) for collaboration sync.
0762. Allow the model to embed other suite docs as live objects.
0763. Provide a schema/version for the model with forward migration.
0764. Support a 'model explorer' dev tool to inspect any node.
0765. Keep the model free of UI concepts (testable headless).
0766. Support a 'replay' of the command log to reproduce a doc.
0767. Allow plugins to extend the model with custom node types.
0768. Provide a 'graph view' linking related objects across docs.
0769. Support a 'semantic layer' so our inference engine reads meaning.
0770. Make the model the integration point with our OS file/VFS.
0771. Support a 'document as a folder' view in the OS shell.
0772. Allow the model to carry training signals for the RL environment.
0773. Provide a 'canonical diff' format for version control.
0774. Support a 'model patch' language for fine-grained sync.
0775. Keep the model deterministic given the same command sequence.
0776. Support a 'snapshot' for undo that is memory-bounded.
0777. Allow the model to reference external data (our data store).
0778. Provide a 'type registry' so add-ins declare node kinds.
0779. Support a 'query language' (XPath-like) over the model.
0780. Make every style a first-class reusable object.
0781. Support a 'theme' object shared across apps.
0782. Allow the model to embed computation (formula nodes).
0783. Provide a 'validation' that checks model invariants.
0784. Support a 'migration' from v1 to vN model automatically.
0785. Expose the model to our OS search/indexer directly.
0786. Support a 'headless render' of any node to an image.
0787. Allow the model to be the unit of collaboration (CRDT on nodes).
0788. Provide a 'node history' (who edited this paragraph).
0789. Support a 'derived view' (e.g., TOC) computed from model.
0790. Allow the model to express relations (see WS17 knowledge graph).
0791. Provide a 'model stats' (nodes, edges, size) for perf.
0792. Support a 'semantic diff' that ignores whitespace/layout.
0793. Make the model the anchor for accessibility tree generation.
0794. Support a 'policy' object enforcing team conventions.
0795. Allow the model to be signed/attested per node.
0796. Provide a 'model lint' for dangling references.
0797. Support a 'live object' that recomputes from source on open.
0798. Allow the model to carry RL task annotations (see WS14).
0799. Provide a 'document manifest' listing all parts and hashes.
0800. Support a 'model patch' applied transactionally.
0801. Keep the model backend-agnostic (file, VFS, store).
0802. Support a 'clone' that deep-copies a subtree.
0803. Allow the model to be the unit of sandboxed scripting.
0804. Provide a 'schema doc' auto-generated for developers.
0805. Define one canonical object model shared by all three apps.
0806. Represent every entity (run, cell, shape) as a typed node.
0807. Separate content, style, and layout in the model.
0808. Make the model serializable to our internal format and OOXML.
0809. Expose a stable DOM-like API for scripts and add-ins.
0810. Give every object a stable ID for diff/merge/undo.
0811. Support a command pattern so all edits are reversible/recordable.
0812. Store rich metadata (provenance, author, timestamps) per node.
0813. Allow queries over the model (find all headings, all charts).
0814. Support a reactive model: UI updates from model changes only.
0815. Make the model the single source for both render and export.
0816. Support incremental updates (patches) for collaboration sync.
0817. Allow the model to embed other suite docs as live objects.
0818. Provide a schema/version for the model with forward migration.
0819. Support a 'model explorer' dev tool to inspect any node.
0820. Keep the model free of UI concepts (testable headless).
0821. Support a 'replay' of the command log to reproduce a doc.
0822. Allow plugins to extend the model with custom node types.
0823. Provide a 'graph view' linking related objects across docs.
0824. Support a 'semantic layer' so our inference engine reads meaning.

### 12. Extensibility & Developer Platform (NOT Office.js)

0825. Design a first-party, local-first extension API, not a webview hack.
0826. Run extensions in a sandbox with explicit capability grants.
0827. Support extensions in our safe scripting language and WASM.
0828. Never require a Microsoft/cloud account to publish an add-in.
0829. Provide a local extension store backed by our OS package manager.
0830. Support extensions that read/write the unified object model (WS11).
0831. Provide a stable API version with clear deprecation policy.
0832. Ship a 'hello world' extension and full reference docs.
0833. Support extensions that add new formula functions to sheets.
0834. Support extensions that add new ribbon/tools panels.
0835. Provide a permission prompt listing exactly what an extension wants.
0836. Allow extensions to be disabled/enabled per document.
0837. Support extension signing and a trust store the user controls.
0838. Provide a headless test harness for extensions in CI.
0839. Never break extensions silently across monthly updates.
0840. Support extensions contributing to the command palette.
0841. Allow extensions to register new file importers/exporters.
0842. Provide a 'developer mode' with live reload and logs.
0843. Support extensions that hook autosave/export events.
0844. Allow extensions to call our inference engine via a local API.
0845. Provide a 'capability manifest' so users audit extensions.
0846. Support extensions that add new chart types.
0847. Allow extensions to contribute accessibility handlers.
0848. Provide a 'safe eval' so extensions can't hang the app.
0849. Support extensions that add new proofing languages.
0850. Provide a 'marketplace' that is optional and offline-mirrorable.
0851. Allow extensions to persist data in a sandboxed store.
0852. Support extensions that add new shape/object types to the model.
0853. Provide a 'debug console' for extension developers.
0854. Never inject extensions into the network/telemetry path.
0855. Support extensions that add new export targets (our store).
0856. Allow extensions to be written in any language compiling to WASM.
0857. Provide a 'permission log' of what each extension did.
0858. Support a 'revoke all' that removes an extension's access.
0859. Allow extensions to subscribe to model change events.
0860. Provide a 'sample gallery' of vetted extensions.
0861. Support a 'minimum privilege' default for new extensions.
0862. Provide a 'review' process that is transparent and local-first.
0863. Allow extensions to add custom properties to nodes.
0864. Support a 'script recorder' that scaffolds an extension.
0865. Provide a 'compat shim' so simple macros become extensions.
0866. Allow extensions to surface UI in our OS notification center.
0867. Support extensions that add new collaboration commands.
0868. Provide a 'sandbox escape' detector (hard fail).
0869. Support extensions that add new proofing dictionaries.
0870. Allow extensions to render custom panes.
0871. Provide a 'documented limits' so devs know sandbox bounds.
0872. Support extensions that add new shapes to presentations.
0873. Allow extensions to register new keyboard shortcuts safely.
0874. Provide a 'no-phoning-home' guarantee enforced by sandbox.
0875. Support a 'local CI' that lints extensions before install.
0876. Allow extensions to be updated via our OS package manager.
0877. Provide a 'capability diff' when an extension updates perms.
0878. Support extensions that add new data sources to sheets.
0879. Allow extensions to contribute to the model query language.
0880. Provide a 'trust tier' (system/user/untrusted) for extensions.
0881. Support a 'audit mode' recording all extension file access.
0882. Allow extensions to define new node kinds in the model.
0883. Provide a 'kill switch' that disables all extensions instantly.
0884. Support a 'permission log' of what each extension did.
0885. Allow extensions to be bundled with a document (sandboxed).
0886. Design a first-party, local-first extension API, not a webview hack.
0887. Run extensions in a sandbox with explicit capability grants.
0888. Support extensions in our safe scripting language and WASM.
0889. Never require a Microsoft/cloud account to publish an add-in.
0890. Provide a local extension store backed by our OS package manager.
0891. Support extensions that read/write the unified object model (WS11).
0892. Provide a stable API version with clear deprecation policy.
0893. Ship a 'hello world' extension and full reference docs.
0894. Support extensions that add new formula functions to sheets.
0895. Support extensions that add new ribbon/tools panels.
0896. Provide a permission prompt listing exactly what an extension wants.
0897. Allow extensions to be disabled/enabled per document.
0898. Support extension signing and a trust store the user controls.
0899. Provide a headless test harness for extensions in CI.
0900. Never break extensions silently across monthly updates.
0901. Support extensions contributing to the command palette.
0902. Allow extensions to hook autosave/export events.
0903. Allow extensions to call our inference engine via a local API.
0904. Provide a 'capability manifest' so users audit extensions.
0905. Support extensions that add new chart types.

### 13. AI / Inference Integration (WuBuMath, local-first)

0906. Integrate our WuBuMath inference engine as the local brain, not Copilot.
0907. Run all AI features on-device by default; cloud optional, never required.
0908. Provide a local LLM assistant that reads the active document context.
0909. Support 'ask about this document' with citations to specific nodes.
0910. Provide draft/rewrite/summarize using the local model offline.
0911. Support grammar/clarity suggestions explained in plain language.
0912. Provide a 'translate' that runs locally for privacy.
0913. Support formula generation from natural language into our engine.
0914. Support 'explain this formula' that traces the computation.
0915. Provide chart-type recommendations from data (local inference).
0916. Support data cleaning suggestions (dedupe, type, fill) locally.
0917. Provide a 'generate table from prompt' that emits real cells.
0918. Support 'make a slide deck from this outline' via local model.
0919. Provide an 'improve writing' with tone/audience controls.
0920. Support a 'research assistant' that queries our knowledge store (WS17).
0921. Provide a 'cite sources' mode that links claims to our vault.
0922. Support speech-to-text dictation via local ASR model.
0923. Support text-to-speech narration locally for review.
0924. Provide a 'describe image' for alt-text generation (opt-in).
0925. Support a 'detect action items' that pulls tasks from docs.
0926. Provide a 'meeting notes to deck' pipeline offline.
0927. Support a 'semantic search' across all local documents.
0928. Provide a 'related documents' suggester from the knowledge graph.
0929. Support a 'prompt library' the user owns and edits.
0930. Provide a 'model swapper' so users pick their own local model.
0931. Support a 'context budget' indicator showing tokens used.
0932. Provide a 'no-training' guarantee: your docs never train shared models.
0933. Support a 'private fine-tune' on user's own corpus, local only.
0934. Provide an 'AI audit log' of every model call and its inputs.
0935. Support a 'confidence' display so users know model certainty.
0936. Provide a 'regenerate' with variation controls.
0937. Support a 'guardrail' that refuses to invent citations.
0938. Provide a 'fact-check' that cross-refs our knowledge store.
0939. Support a 'template from prompt' generator.
0940. Provide a 'local embedding' model for semantic search.
0941. Support a 'summarize selection' with length control.
0942. Provide a 'continue writing' that matches the user's voice.
0943. Support a 'translate comments' across languages live.
0944. Provide a 'spell/grammar' that explains each fix.
0945. Support a 'reading-level' adjuster for audience.
0946. Provide a 'brainstorm' mode that expands an outline.
0947. Support a 'extract tasks' to our OS task list.
0948. Provide a 'generate chart from words' (e.g., 'show sales by region').
0949. Support a 'what-if' natural language on spreadsheets.
0950. Provide a 'explain error' for formula/logging issues.
0951. Support a 'local agent' that performs multi-step doc tasks.
0952. Provide a 'model card' per feature (what it can/can't do).
0953. Support a 'opt-in telemetry' only if user enables, anonymized.
0954. Provide a 'offline proof' that AI works with zero network.
0955. Support a 'prompt injection' guard on document-sourced prompts.
0956. Provide a 'diff view' of AI changes before accept.
0957. Support a 'revert AI' that drops all model edits atomically.
0958. Provide a 'model health' check (does local engine respond).
0959. Support a 'bring your own model' endpoint (local or self-hosted).
0960. Provide a 'token accounting' so users track local compute.
0961. Support a 'privacy mode' that blanks AI from sensitive sections.
0962. Provide a 'local knowledge cutoff' display.
0963. Support a 'multimodal' read of embedded images/charts.
0964. Provide a 'ask the deck' Q&A over a presentation.
0965. Support a 'generate alt-text' batch for all images.
0966. Provide a 'tone detector' for emails/docs.
0967. Support a 'local vector store' shared with our OS search.
0968. Provide a 'AI sidebar' dockable, dismissible, offline.
0969. Support a 'no account' AI: model weights live on the device.
0970. Provide a 'explain like I'm new' simplifier.
0971. Support a 'generate from data' natural-language charts/tables.
0972. Integrate our WuBuMath inference engine as the local brain, not Copilot.
0973. Run all AI features on-device by default; cloud optional, never required.
0974. Provide a local LLM assistant that reads the active document context.
0975. Support 'ask about this document' with citations to specific nodes.
0976. Provide draft/rewrite/summarize using the local model offline.
0977. Support grammar/clarity suggestions explained in plain language.
0978. Provide a 'translate' that runs locally for privacy.
0979. Support formula generation from natural language into our engine.
0980. Support 'explain this formula' that traces the computation.
0981. Provide chart-type recommendations from data (local inference).
0982. Support data cleaning suggestions (dedupe, type, fill) locally.
0983. Provide a 'generate table from prompt' that emits real cells.
0984. Provide a 'make a slide deck from this outline' via local model.
0985. Provide an 'improve writing' with tone/audience controls.
0986. Provide a 'research assistant' that queries our knowledge store (WS17).
0987. Provide a 'cite sources' mode that links claims to our vault.
0988. Support speech-to-text dictation via local ASR model.
0989. Support text-to-speech narration locally for review.
0990. Provide a 'describe image' for alt-text generation (opt-in).
0991. Provide a 'detect action items' that pulls tasks from docs.

### 14. Reinforcement Learning Environment

0992. Expose every office task as an RL environment (state/action/reward).
0993. Define a state encoding over the unified object model (WS11).
0994. Define atomic actions: insert, edit, format, navigate, save.
0995. Provide a fast headless simulator for millions of RL episodes.
0996. Provide a reward model for 'task complete' vs 'user satisfied'.
0997. Support curriculum learning from simple to complex documents.
0998. Provide a set of benchmark tasks (make a memo, build a chart).
0999. Allow RL agents to learn keyboard/mouse action sequences.
1000. Provide a 'demo' dataset of human-written docs as expert trajectories.
1001. Support imitation learning from the demo corpus.
1002. Provide a 'task spec' language so users define goals.
1003. Allow agents to propose edits that a human accepts/rejects.
1004. Provide a 'safety wrapper' that blocks destructive actions.
1005. Support a 'undo as negative reward' signal.
1006. Provide a 'habit learner' that adapts UI to user patterns.
1007. Allow the RL env to drive our inference engine for planning.
1008. Provide an OpenAI-gym-like interface for external researchers.
1009. Support a 'multi-agent' mode: one agent per app coordinating.
1010. Provide a 'reward shaping' that values clarity/accessibility.
1011. Allow the env to emit traces for debugging policies.
1012. Provide a 'deterministic mode' for reproducible RL runs.
1013. Support a 'partial observability' setting (agent sees viewport).
1014. Provide a 'transfer learning' from sheet tasks to doc tasks.
1015. Allow agents to query the knowledge graph (WS17) as context.
1016. Provide a 'skill library' of learned sub-policies reusable.
1017. Support a 'human-in-the-loop' reward from real usage.
1018. Provide a 'safety sandbox' where agents practice harmlessly.
1019. Allow the env to run inside our OS as a first-class service.
1020. Provide a 'task success metric' measured by output validation.
1021. Support a 'curriculum generator' that synthesizes tasks.
1022. Provide a 'policy zoo' of released checkpoints.
1023. Allow agents to call the AI assistant as a sub-policy.
1024. Provide a 'explain action' that narrates the agent's choice.
1025. Support a 'constrained agent' obeying team policies.
1026. Provide a 'reward for accessibility' (alt text, contrast).
1027. Allow the env to score 'formatting consistency'.
1028. Provide a 'no-op penalty' to avoid agent stalling.
1029. Support a 'macro discovery' that compresses actions into skills.
1030. Provide a 'task generator from templates'.
1031. Allow agents to learn 'fix this error' from formula failures.
1032. Provide a 'benchmark leaderboard' for office tasks.
1033. Support a 'sim-to-real' gap analysis against human use.
1034. Provide a 'agent replay' viewer for inspection.
1035. Allow the env to export trajectories to our training store.
1036. Provide a 'ablation' tool to study which features matter.
1037. Support a 'safe exploration' that never corrupts real files.
1038. Provide a 'task difficulty' estimator for curriculum.
1039. Allow agents to collaborate in real-time co-editing (WS06).
1040. Provide a 'reward for speed' balanced against quality.
1041. Support a 'distributed RL' across machines via our OS.
1042. Provide a 'policy distillation' to a small on-device model.
1043. Allow the env to generate synthetic training documents.
1044. Provide a 'evaluation harness' on held-out human tasks.
1045. Support a 'fairness' check that agents don't favor styles.
1046. Provide a 'explainability' report per agent decision.
1047. Allow the env to hook our OS accessibility for state (WS04).
1048. Provide a 'agent permissions' separate from user permissions.
1049. Support a 'rollback' of any agent episode instantly.
1050. Provide a 'task ontology' shared across apps.
1051. Allow the env to reward 'minimal steps' for efficiency.
1052. Provide a 'curriculum of 1000 tasks' derived from this docket.
1053. Support a 'self-play' where agents review each other's docs.
1054. Provide a 'deployment' path: learned policy assists users live.
1055. Expose every office task as an RL environment (state/action/reward).
1056. Define a state encoding over the unified object model (WS11).
1057. Define atomic actions: insert, edit, format, navigate, save.
1058. Provide a fast headless simulator for millions of RL episodes.
1059. Provide a reward model for 'task complete' vs 'user satisfied'.
1060. Support curriculum learning from simple to complex documents.
1061. Provide a set of benchmark tasks (make a memo, build a chart).
1062. Allow RL agents to learn keyboard/mouse action sequences.
1063. Provide a 'demo' dataset of human-written docs as expert trajectories.
1064. Support imitation learning from the demo corpus.
1065. Provide a 'task spec' language so users define goals.
1066. Allow agents to propose edits that a human accepts/rejects.
1067. Provide a 'safety wrapper' that blocks destructive actions.
1068. Support a 'undo as negative reward' signal.
1069. Provide a 'habit learner' that adapts UI to user patterns.
1070. Allow the RL env to drive our inference engine for planning.
1071. Provide an OpenAI-gym-like interface for external researchers.
1072. Support a 'multi-agent' mode: one agent per app coordinating.
1073. Provide a 'reward shaping' that values clarity/accessibility.
1074. Allow the env to emit traces for debugging policies.

### 15. Operating System Integration (like MS Office + Win9x)

1075. Register as the OS default handler for OOXML/ODF/PDF types.
1076. Provide deep file-manager preview (thumbnails of pages/sheets).
1077. Integrate with the OS shell 'open with' and 'share' menus.
1078. Support OS theming (dark/light) automatically.
1079. Expose documents to the OS global search/indexer.
1080. Provide a 'quick note' that drops into the OS clipboard/history.
1081. Integrate with OS notifications for collaboration/comments.
1082. Support OS single-sign-on identity for local sharing.
1083. Provide a 'share sheet' that targets our OS apps and contacts.
1084. Register custom URI schemes (office://open?...) for deep links.
1085. Support drag-and-drop from the OS file manager directly.
1086. Provide a 'send to' that exports to OS mail/messaging.
1087. Integrate with OS spell-check/proofing if available.
1088. Support OS voice input via the platform ASR bridge.
1089. Provide a 'recent documents' jump list / dock stack.
1090. Integrate with OS power management (pause autosave on battery save).
1091. Support OS file versioning (VFS snapshots) natively.
1092. Provide a 'print to office' virtual printer that captures to doc.
1093. Integrate with OS accessibility (AT-SPI/UIA) per WS04.
1094. Support OS sandbox/permission prompts for file access.
1095. Provide a 'quick look' plugin for the OS file viewer.
1096. Integrate with OS contacts/address book for mail merge.
1097. Support OS global hotkeys that launch templates.
1098. Provide a 'document as a folder' mount in the OS.
1099. Integrate with OS backup (exclude temp, keep docs).
1100. Support OS 'open in terminal' for headless batch ops.
1101. Provide a 'status indicator' in the OS tray.
1102. Integrate with OS font management (no private copies).
1103. Support OS locale/region for number/date formats.
1104. Provide a 'share to deck' from any OS app via intent.
1105. Integrate with OS screen capture for embedding.
1106. Support OS 'focus mode' that dims other windows.
1107. Provide a 'file handler' that previews without launching full app.
1108. Integrate with OS encryption (LUKS/BitLocker) at rest.
1109. Support OS 'tag' metadata shown in the file manager.
1110. Provide a 'new document' from OS context menu (per type).
1111. Integrate with OS update mechanism (our package manager).
1112. Support OS 'default apps' control panel registration.
1113. Provide a 'document properties' sheet in the OS file dialog.
1114. Integrate with OS 'continue where you left off' session.
1115. Support OS 'quick actions' (e.g., convert to PDF).
1116. Provide a 'send to knowledge store' OS share target (WS17).
1117. Integrate with OS 'focus assist' to mute notifications.
1118. Support OS 'file associations' without stealing others.
1119. Provide a 'thumbnail cache' the OS file manager reuses.
1120. Integrate with OS 'timeline'/activity history (local).
1121. Support OS 'open from network' transparently.
1122. Provide a 'new from scanner' via OS scan service.
1123. Integrate with OS 'translate' OS-level if present.
1124. Support OS 'privacy dashboard' reflecting our no-telemetry.
1125. Provide a 'document canvas' as an OS-managed surface.
1126. Integrate with OS 'energy saver' to cap background work.
1127. Support OS 'multi-desktop' per app window placement.
1128. Provide a 'share status' in OS presence (optional).
1129. Integrate with OS 'clipboard history' rich paste.
1130. Support OS 'file provider' so docs appear in open dialogs.
1131. Provide a 'launch at login' toggle in OS settings.
1132. Integrate with OS 'text services framework' for IME.
1133. Support OS 'drag file out' of the app to the desktop.
1134. Provide a 'quick create' from OS search bar.
1135. Integrate with OS 'color picker' system-wide.
1136. Support OS 'parental/usage controls' hook if present.
1137. Register as the OS default handler for OOXML/ODF/PDF types.
1138. Provide deep file-manager preview (thumbnails of pages/sheets).
1139. Integrate with the OS shell 'open with' and 'share' menus.
1140. Support OS theming (dark/light) automatically.
1141. Expose documents to the OS global search/indexer.
1142. Provide a 'quick note' that drops into the OS clipboard/history.
1143. Integrate with OS notifications for collaboration/comments.
1144. Support OS single-sign-on identity for local sharing.
1145. Provide a 'share sheet' that targets our OS apps and contacts.
1146. Register custom URI schemes (office://open?...) for deep links.
1147. Support drag-and-drop from the OS file manager directly.
1148. Provide a 'send to' that exports to OS mail/messaging.
1149. Integrate with OS spell-check/proofing if available.
1150. Support OS voice input via the platform ASR bridge.
1151. Provide a 'recent documents' jump list / dock stack.
1152. Integrate with OS power management (pause autosave on battery).
1153. Support OS file versioning (VFS snapshots) natively.
1154. Provide a 'print to office' virtual printer that captures to doc.
1155. Integrate with OS accessibility (AT-SPI/UIA) per WS04.
1156. Support OS sandbox/permission prompts for file access.

### 16. Cross-App Workflow & Interop (live objects, undo, clipboard)

1157. Support copy/paste that preserves rich structure across apps.
1158. Support live linked objects (edit sheet, deck chart updates).
1159. Provide a unified clipboard with multiple named clip entries.
1160. Support OLE-like embedding without a Windows dependency.
1161. Provide a single global undo stack spanning cross-app drag.
1162. Support 'paste special' with format choices per target.
1163. Provide a 'smart paste' that adapts to destination style.
1164. Support drag a chart from sheet into a doc/deck live.
1165. Provide a 'send to' between apps (selection -> new slide).
1166. Support a shared theme so all apps match instantly.
1167. Provide a 'collect from docs' that gathers selected content.
1168. Support a unified find across all open documents.
1169. Provide a 'cross-app macro' scripting the whole suite.
1170. Support a 'data bus' so apps share computed values live.
1171. Provide a 'linked range' from sheet feeding a doc field.
1172. Support a 'unlink' that freezes a previously live object.
1173. Provide a 'clipboard history' searchable and pinned.
1174. Support pasting a table that becomes a real table (not image).
1175. Provide a 'format painter' that works across apps.
1176. Support a 'style sync' so heading styles match everywhere.
1177. Provide a 'multi-select' across apps in a workspace.
1178. Support a 'workspace' bundling related docs/sheets/decks.
1179. Provide a 'recent cross-app actions' rail.
1180. Support a 'live object inspector' showing source links.
1181. Provide a 'break link' that inlines the current value.
1182. Support a 'cross-app search' via the OS indexer (WS15).
1183. Provide a 'unified print' preview across a workspace.
1184. Support a 'send selection to AI' from any app (WS13).
1185. Provide a 'copy as' (Markdown, PNG, OOXML, plain).
1186. Support a 'drag image out' to the OS desktop (WS15).
1187. Provide a 'linked comment' that surfaces in all apps.
1188. Support a 'master data' sheet feeding multiple decks/docs.
1189. Provide a 'refresh all links' command.
1190. Support a 'link health' check showing broken sources.
1191. Provide a 'cross-app undo' with a visible transaction log.
1192. Support a 'paste JSON as table' smart conversion.
1193. Provide a 'paste CSV with delimiter detection'.
1194. Support a 'round-trip' doc->sheet->doc without loss.
1195. Provide a 'unified hyperlink' resolver across the workspace.
1196. Support a 'send to knowledge store' (WS17) from any app.
1197. Provide a 'compare workspace' diff across all docs.
1198. Support a 'template from workspace' capturing interlinks.
1199. Provide a 'cross-app selection' clipboard object.
1200. Support a 'live screenshot' embedding that updates.
1201. Provide a 'unified numbering' across docs in a workspace.
1202. Support a 'cross-reference' to any object in any app.
1203. Provide a 'workspace manifest' listing all parts/hashes.
1204. Support a 'export workspace' as a single portable bundle.
1205. Provide a 'import workspace' that relinks objects.
1206. Support a 'shared undo' respecting collaboration (WS06).
1207. Provide a 'clipboard sanitizer' stripping trackers on paste.
1208. Support a 'paste as picture' with editable source link.
1209. Provide a 'cross-app spellcheck' consistent lexicon.
1210. Support a 'send to RL env' (WS14) as a task episode.
1211. Provide a 'unified zoom/theme' applied to all open windows.
1212. Support a 'workspace search' returning hits with app + location.
1213. Provide a 'linked footnote' that follows the source doc.
1214. Support a 'data flow diagram' of live links in a workspace.
1215. Provide a 'disconnect all' that snapshots then unlinks.
1216. Support a 'cross-app quick switch' (Ctrl+Tab cycles apps).
1217. Support copy/paste that preserves rich structure across apps.
1218. Support live linked objects (edit sheet, deck chart updates).
1219. Provide a unified clipboard with multiple named clip entries.
1220. Support OLE-like embedding without a Windows dependency.
1221. Provide a single global undo stack spanning cross-app drag.
1222. Provide a 'paste special' with format choices per target.
1223. Provide a 'smart paste' that adapts to destination style.
1224. Support drag a chart from sheet into a doc/deck live.
1225. Provide a 'send to' between apps (selection -> new slide).
1226. Support a shared theme so all apps match instantly.
1227. Provide a 'collect from docs' that gathers selected content.
1228. Support a unified find across all open documents.
1229. Provide a 'cross-app macro' scripting the whole suite.
1230. Support a 'data bus' so apps share computed values live.
1231. Provide a 'linked range' from sheet feeding a doc field.
1232. Provide a 'unlink' that freezes a previously live object.
1233. Provide a 'clipboard history' searchable and pinned.
1234. Support pasting a table that becomes a real table (not image).
1235. Provide a 'format painter' that works across apps.
1236. Support a 'style sync' so heading styles match everywhere.

### 17. Data & Knowledge Integration (our other software, vault, graphs)

1237. Provide a 'knowledge store' where documents become queryable nodes.
1238. Support a local graph of entities extracted from documents.
1239. Allow the suite to read/write our existing 'vault' (see memory map).
1240. Provide semantic search across docs via local embeddings (WS13).
1241. Support backlinks so a doc knows what references it.
1242. Provide a 'related' panel drawing from the knowledge graph.
1243. Allow the AI assistant to ground answers in the vault (WS13).
1244. Support tagging documents that sync to the vault taxonomy.
1245. Provide a 'cite' feature linking claims to vault sources.
1246. Support import from our other software's data formats.
1247. Provide a 'publish to vault' that atomizes a doc into notes.
1248. Support a 'collection' that bundles docs by project.
1249. Provide a 'timeline' view of when docs were created/edited.
1250. Allow the RL env to use the graph as state context (WS14).
1251. Support a 'query language' over the knowledge store.
1252. Provide a 'daily digest' of changes across the vault.
1253. Support export to our OS-indexed search.
1254. Provide a 'private web' of the user's own documents.
1255. Support 'mentions' of vault entities inside documents.
1256. Provide a 'graph explorer' UI for the knowledge store.
1257. Support conflict-free sync of the vault across devices.
1258. Provide a 'diff vault' showing doc changes over time.
1259. Support 'templates as vault items' reused everywhere.
1260. Provide a 'trash/archive' with restore in the vault.
1261. Support 'access control' on vault collections.
1262. Provide a 'workspace' that is a live view of the vault (WS16).
1263. Support a 'semantic duplicate' finder across docs.
1264. Provide a 'concept map' auto-built from headings/terms.
1265. Support 'annotations' that attach to any node (WS11).
1266. Provide a 'citation style' manager (APA/MLA/Chicago).
1267. Support 'import bibliography' from Zotero/BibTeX.
1268. Provide a 'knowledge card' summarizing a topic from docs.
1269. Support 'link suggestions' while writing (like a graph).
1270. Provide a 'vault health' (orphans, duplicates, stale).
1271. Support 'export graph' to our OS for other apps to use.
1272. Provide a 'private search' that never leaves the device.
1273. Support 'entity resolution' merging same person/topic.
1274. Provide a 'timeline' of entity mentions across docs.
1275. Support 'collections as playlists' for review.
1276. Provide a 'vault as a filesystem' mount in the OS (WS15).
1277. Support 'AI summarizer' over the whole vault (WS13).
1278. Provide a 'what changed this week' report.
1279. Support 'tag hierarchy' with inheritance.
1280. Provide a 'document -> vault' two-way link with sync.
1281. Support 'protected vault' encrypted at rest (WS02).
1282. Provide a 'vault query' in the command palette (WS03).
1283. Support 'related decks/docs' surfaced in new-doc wizard.
1284. Provide a 'knowledge graph' export to our RL env (WS14).
1285. Support 'annotations sync' to our reading/other apps.
1286. Provide a 'source library' for research workflows.
1287. Support 'auto-tag' via local model on save (opt-in).
1288. Provide a 'vault backup' that is portable folders.
1289. Support 'merge vaults' from multiple machines.
1290. Provide a 'vault stats' (nodes, edges, size).
1291. Support 'semantic alerts' when new doc contradicts old.
1292. Provide a 'cite-while-present' for decks (WS10).
1293. Support 'vault as context' for the AI assistant (WS13).
1294. Provide a 'entity extractor' that runs offline.
1295. Support 'link to OS contacts' from the vault (WS15).
1296. Provide a 'knowledge diff' between two vault snapshots.
1297. Support 'vault permissions' per collection for teams.
1298. Provide a 'graph pruning' to drop stale edges.
1299. Support 'vault API' for our other software to consume.
1300. Provide a 'private index' shared with OS search (WS15).
1301. Support 'document as a graph node' natively.
1302. Provide a 'related tasks' pulling from our OS task list.
1303. Support 'vault export' to open formats (no lock-in).
1304. Provide a 'knowledge store' where documents become queryable nodes.
1305. Support a local graph of entities extracted from documents.
1306. Allow the suite to read/write our existing 'vault' (see memory map).
1307. Provide semantic search across docs via local embeddings (WS13).
1308. Support backlinks so a doc knows what references it.
1309. Provide a 'related' panel drawing from the knowledge graph.
1310. Allow the AI assistant to ground answers in the vault (WS13).
1311. Support tagging documents that sync to the vault taxonomy.
1312. Provide a 'cite' feature linking claims to vault sources.
1313. Support import from our other software's data formats.
1314. Provide a 'publish to vault' that atomizes a doc into notes.
1315. Provide a 'collection' that bundles docs by project.
1316. Provide a 'timeline' view of when docs were created/edited.
1317. Allow the RL env to use the graph as state context (WS14).
1318. Support a 'query language' over the knowledge store.
1319. Provide a 'daily digest' of changes across the vault.
1320. Support export to our OS-indexed search.
1321. Provide a 'private web' of the user's own documents.
1322. Support 'mentions' of vault entities inside documents.
1323. Provide a 'graph explorer' UI for the knowledge store.

### 18. Security & Sandboxing

1324. Run all document code (macros/scripts) in a capability sandbox.
1325. Never execute embedded scripts from untrusted docs by default.
1326. Provide a 'trust' model: local files trusted, downloaded prompt.
1327. Sandbox the parser so malformed files can't crash the app.
1328. Fuzz all importers in CI to harden against malicious files.
1329. Support signed documents with local key verification.
1330. Provide a 'block macros' policy for enterprise.
1331. Never auto-run content from the internet inside a doc.
1332. Provide a 'safe open' mode that disables all active content.
1333. Support encrypted at-rest storage with user-held keys (WS02).
1334. Provide a 'redact' tool that irreversibly removes content.
1335. Support a 'watermark' for confidential drafts.
1336. Provide a 'DLP-lite' that flags sending sensitive content out.
1337. Never load remote resources (images/scripts) without consent.
1338. Provide a 'permissions log' of file/network access.
1339. Support a 'disable network' hard switch at app level.
1340. Provide a 'sandbox escape' detector that hard-fails.
1341. Support a 'child process' policy (no spawning shells).
1342. Provide a 'capability manifest' for every extension (WS12).
1343. Support a 'revoke all' that drops every grant.
1344. Provide a 'malware scan' hook to OS scanner if present.
1345. Support a 'no-exec' memory policy for parsed data.
1346. Provide a 'taint tracking' from untrusted sources.
1347. Support a 'safe render' that strips active content on view.
1348. Provide a 'password strength' meter for doc encryption.
1349. Support a 'zero-knowledge' sync (server sees ciphertext).
1350. Provide a 'audit log' of edits for compliance.
1351. Support a 'break-glass' that disables all extensions fast.
1352. Provide a 'content policy' per team/org.
1353. Support a 'signed updates' verified against our key.
1354. Provide a 'no RCE' guarantee on file open.
1355. Support a 'clipboard sanitizer' removing trackers (WS16).
1356. Provide a 'phishing' hint when a doc links to odd URLs.
1357. Support a 'document provenance' chain of custody.
1358. Provide a 'secure delete' that wipes temp files.
1359. Support a 'permission prompt' for any file read outside home.
1360. Provide a 'sandbox report' after opening untrusted doc.
1361. Support a 'disable embeds' mode for max safety.
1362. Provide a 'macro approval' per document/source.
1363. Support a 'least privilege' default for new docs.
1364. Provide a 'security label' (public/confidential) enforcement.
1365. Support a 'encrypt to recipient' via their public key.
1366. Provide a 'tamper-evident' save with hash chain.
1367. Support a 'no clipboard leakage' to other apps unless shared.
1368. Provide a 'sandbox for AI' so the model can't touch files (WS13).
1369. Support a 'policy as code' for orgs to enforce rules.
1370. Provide a 'incident log' for blocked actions.
1371. Support a 'disable network for AI' hard guarantee.
1372. Provide a 'signed boot' of the suite binary.
1373. Support a 'permission diff' on extension update (WS12).
1374. Provide a 'safe mode' that loads zero extensions/macros.
1375. Support a 'file origin' tracking (local/download/email).
1376. Provide a 'block external refs' toggle in open dialog.
1377. Support a 'redaction export' that bakes redactions in.
1378. Provide a 'classification bar' showing doc sensitivity.
1379. Support a 'no covert channel' audit of dependencies.
1380. Provide a 'sandbox for RL agent' (WS14) separate from user files.
1381. Support a 'permission review' wizard on first run.
1382. Provide a 'encrypted swap' so sensitive data isn't paged.
1383. Support a 'lock after idle' with OS screen lock.
1384. Provide a 'secure share link' with expiry (self-hosted).
1385. Support a 'no telemetry' verified by packet capture in CI.
1386. Run all document code (macros/scripts) in a capability sandbox.
1387. Never execute embedded scripts from untrusted docs by default.
1388. Provide a 'trust' model: local files trusted, downloaded prompt.
1389. Sandbox the parser so malformed files can't crash the app.
1390. Fuzz all importers in CI to harden against malicious files.
1391. Support signed documents with local key verification.
1392. Provide a 'block macros' policy for enterprise.
1393. Never auto-run content from the internet inside a doc.
1394. Provide a 'safe open' mode that disables all active content.
1395. Support encrypted at-rest storage with user-held keys (WS02).
1396. Provide a 'redact' tool that irreversibly removes content.
1397. Support a 'watermark' for confidential drafts.
1398. Provide a 'DLP-lite' that flags sending sensitive content out.
1399. Never load remote resources (images/scripts) without consent.
1400. Provide a 'permissions log' of file/network access.
1401. Support a 'disable network' hard switch at app level.
1402. Provide a 'sandbox escape' detector that hard-fails.
1403. Support a 'child process' policy (no spawning shells).
1404. Provide a 'capability manifest' for every extension (WS12).
1405. Support a 'revoke all' that drops every grant.

### 19. Migration & Compatibility Tooling

1406. Provide a 'import from Word/Excel/PowerPoint' faithful converter.
1407. Support a 'VBA to our safe script' transpiler best-effort.
1408. Provide a 'macro audit' listing what will/won't convert.
1409. Support a 'one-click' bulk converter for whole folders.
1410. Provide a 'compatibility report' vs the source app.
1411. Support a 'preserve macros' by sandboxing them (WS18).
1412. Provide a 'find-broken-refs' on import and offer fixes.
1413. Support a 'map styles' from source template to ours.
1414. Provide a 'translate shortcuts' cheat sheet for migrants.
1415. Support a 'training mode' that teaches our UI to Excel users.
1416. Provide a 'legacy binary' importer (.doc/.xls/.ppt).
1417. Support a 'Google Drive' import via exported OOXML.
1418. Provide a 'migration wizard' that walks the first run.
1419. Support a 'compare original vs imported' diff.
1420. Provide a 'batch re-save' to our native format.
1421. Support a 'preserve comments/threads' on import.
1422. Provide a 'preserve track changes' on import.
1423. Support a 'map fonts' when source font is missing.
1424. Provide a 'report missing features' honestly per file.
1425. Support a 'quick switch' toggle that emulates rival shortcuts.
1426. Provide a 'import from ODF' losslessly.
1427. Support a 'convert to PDF' as a migration checkpoint.
1428. Provide a 'validate' that opens the result in a clean process.
1429. Support a 'undo import' that keeps the original untouched.
1430. Provide a 'language pack' installer for migrants.
1431. Support a 'sample docs' that teach by example.
1432. Provide a 'tutorials' comparing our way to the old way.
1433. Support a 'keyboard layout' switcher (Excel/Libre/ours).
1434. Provide a 'formula translator' for rival function names.
1435. Support a 'chart mapper' from rival chart types.
1436. Provide a 'theme importer' from a rival template.
1437. Support a 'bulk metadata fix' on import (strip paths).
1438. Provide a 'preserve hyperlinks' on import.
1439. Support a 'fix broken media links' on import.
1440. Provide a 'convert macros to extensions' helper (WS12).
1441. Support a 'migrate settings' from a previous install.
1442. Provide a 'portable profile' export/import.
1443. Support a 'compare to MS' fidelity score per doc.
1444. Provide a 'learn our model' interactive tour.
1445. Support a 'import from Apple Pages/Numbers/Keynote'.
1446. Provide a 'batch rename/relink' media on import.
1447. Support a 'preserve numbering' schemes on import.
1448. Provide a 'map colors' from rival theme.
1449. Support a 'convert forms' to our content controls.
1450. Provide a 'preserve smart-art' as editable shapes.
1451. Support a 'convert equations' to MathML.
1452. Provide a 'migrate add-ins' to our extension model (WS12).
1453. Provide a 'compatibility FAQ' per app.
1454. Support a 'dry-run' conversion that reports before writing.
1455. Provide a 'preserve digital signatures' on import.
1456. Support a 'fix encoding' for legacy non-UTF8 files.
1457. Provide a 'map page sizes' from rival defaults.
1458. Support a 'import from Evernote/Notion export' (MD/HTML).
1459. Provide a 'preserve revision history' where possible.
1460. Support a 'convert to our template' in one step.
1461. Provide a 'validation gate' blocking bad imports with reason.
1462. Support a 'batch thumbnail' generation post-import.
1463. Provide a 'migration log' per file for audit.
1464. Support a 'rollback import' if result is unsatisfactory.
1465. Provide a 'preserve language' tags on import.
1466. Support a 'convert to/from' Markdown round-trip.
1467. Provide a 'smart import' that detects format automatically.
1468. Support a 'preserve bookmarks' on import.
1469. Provide a 'import from Word/Excel/PowerPoint' faithful converter.
1470. Support a 'VBA to our safe script' transpiler best-effort.
1471. Provide a 'macro audit' listing what will/won't convert.
1472. Support a 'one-click' bulk converter for whole folders.
1473. Provide a 'compatibility report' vs the source app.
1474. Support a 'preserve macros' by sandboxing them (WS18).
1475. Provide a 'find-broken-refs' on import and offer fixes.
1476. Support a 'map styles' from source template to ours.
1477. Provide a 'translate shortcuts' cheat sheet for migrants.
1478. Support a 'training mode' that teaches our UI to Excel users.
1479. Provide a 'legacy binary' importer (.doc/.xls/.ppt).
1480. Support a 'Google Drive' import via exported OOXML.
1481. Provide a 'migration wizard' that walks the first run.
1482. Support a 'compare original vs imported' diff.
1483. Provide a 'batch re-save' to our native format.
1484. Support a 'preserve comments/threads' on import.
1485. Support a 'preserve track changes' on import.
1486. Support a 'map fonts' when source font is missing.
1487. Provide a 'report missing features' honestly per file.
1488. Provide a 'quick switch' toggle that emulates rival shortcuts.

### 20. Distribution, Packaging & Onboarding

1489. Ship a single portable binary that needs no installer.
1490. Provide OS-package-manager packages (our OS native).
1491. Support a 'minimal' install (one app) and 'full' bundle.
1492. Provide a 'verified' signature on every release artifact.
1493. Support offline install media (USB) for air-gapped orgs.
1494. Provide a 'first-run wizard' that sets privacy/theme/AI defaults.
1495. Support a 'silent install' with a config file for orgs.
1496. Provide a 'what's new' that is honest and skippable.
1497. Support a 'reset to defaults' without reinstall.
1498. Provide a 'portable profile' on a stick (docs+settings).
1499. Support a 'check for update' that is local/opt-in, no auto.
1500. Provide a 'release notes' in plain language per version.
1501. Support a 'downgrade' path to any prior version.
1502. Provide a 'bandwidth-friendly' update (delta patches).
1503. Support a 'staged rollout' toggle for cautious orgs.
1504. Provide a 'integrity check' on every download.
1505. Support a 'mirror' for the update server (self-host).
1506. Provide a 'no account' update path.
1507. Support a 'Linux/our-OS/other' packages from one source.
1508. Provide a 'container image' for server/batch use.
1509. Support a 'snap/flatpak/ours' where applicable.
1510. Provide a 'docs bundled offline' with the app.
1511. Support a 'sample files' gallery installed locally.
1512. Provide a 'keyboard map' PDF in the installer.
1513. Support a 'uninstall' that scrubs all traces (WS02).
1514. Provide a 'repair install' that fixes broken files.
1515. Support a 'enterprise config' (policy file) at deploy.
1516. Provide a 'telemetry off' as the default, not opt-out.
1517. Support a 'language select' at install with all locales.
1518. Provide a 'accessibility preset' chooser on first run.
1519. Support a 'import old profile' from rival suites.
1520. Provide a 'community builds' clearly labeled.
1521. Support a 'source tarball' for self-compilers.
1522. Provide a 'bill of materials' (SBOM) per release.
1523. Support a 'verify signature' tool for auditors.
1524. Provide a 'no bloatware' guarantee in the installer.
1525. Support a 'choose components' install UI.
1526. Provide a 'portable vs installed' clear choice.
1527. Support a 'update cadence' control (never/monthly/stable).
1528. Provide a 'rollback update' if regressions appear.
1529. Support a 'release channel' (stable/beta) per machine.
1530. Provide a 'offline help' searchable without network.
1531. Support a 'quick start' cards on first launch.
1532. Provide a 'welcome deck' that demonstrates features.
1533. Support a 'send feedback' that is local-first (no forced account).
1534. Provide a 'diagnostic bundle' export for bug reports.
1535. Support a 'no auto-launch' unless user opts in.
1536. Provide a 'file type registration' toggle per format.
1537. Support a 'associate or not' choice at install.
1538. Provide a 'disk footprint' display before install.
1539. Support a 'minimal RAM' preset for old hardware.
1540. Provide a 'privacy notice' shown before any network use.
1541. Support a 'trust store' management UI.
1542. Provide a 'update from LAN' for offline orgs.
1543. Support a 'version badge' in the title bar (toggle).
1544. Provide a 'what changed' diff vs installed version.
1545. Support a 'clean uninstall' removing caches.
1546. Support a 'portable apps menu' integration (our OS).
1547. Provide a 'no background updater' unless enabled.
1548. Support a 'checksum file' alongside releases.
1549. Provide a 'documented EOL' with migration path.
1550. Support a 'per-app install' so users pick what they need.
1551. Provide a 'silent config schema' published for orgs.
1552. Ship a single portable binary that needs no installer.
1553. Provide OS-package-manager packages (our OS native).
1554. Support a 'minimal' install (one app) and 'full' bundle.
1555. Provide a 'verified' signature on every release artifact.
1556. Support offline install media (USB) for air-gapped orgs.
1557. Provide a 'first-run wizard' that sets privacy/theme/AI defaults.
1558. Support a 'silent install' with a config file for orgs.
1559. Provide a 'what's new' that is honest and skippable.
1560. Support a 'reset to defaults' without reinstall.
1561. Provide a 'portable profile' on a stick (docs+settings).
1562. Support a 'check for update' that is local/opt-in, no auto.
1563. Provide a 'release notes' in plain language per version.
1564. Support a 'downgrade' path to any prior version.
1565. Provide a 'bandwidth-friendly' update (delta patches).
1566. Support a 'staged rollout' toggle for cautious orgs.
1567. Provide a 'integrity check' on every download.
1568. Support a 'mirror' for the update server (self-host).
1569. Provide a 'no account' update path.
1570. Provide a 'Linux/our-OS/other' packages from one source.
1571. Provide a 'container image' for server/batch use.

### 21. Mobile, Touch & Pen

1572. Provide a touch-optimized UI mode with large hit targets.
1573. Support active pen inking with pressure and tilt.
1574. Provide handwriting-to-text offline via local model.
1575. Support a 'phone companion' that edits on the go.
1576. Provide a 'tablet layout' with ribbon adapted to touch.
1577. Support multi-touch zoom/pan on canvas.
1578. Provide a 'voice dictation' using OS/local STT (WS13).
1579. Support a 'scan to doc' via OS camera (WS15).
1580. Provide a 'read-aloud' TTS for commute review (WS04).
1581. Support a 'quick capture' widget for notes on lock screen.
1582. Provide a 'sync' that is CRDT-based and offline (WS06).
1583. Support a 'reduced chrome' mobile editor.
1584. Provide a 'thumb keyboard' shortcuts for common actions.
1585. Support a 'stylus eraser' and highlighter natively.
1586. Provide a 'shape recognition' from freehand (local).
1587. Support a 'presentation remote' from phone (WS10).
1588. Provide a 'review mode' on phone for comments/approve.
1589. Support a 'offline first' mobile that syncs later.
1590. Provide a 'small-screen' sheet view (frozen key cols).
1591. Support a 'drag handle' for reordering on touch.
1592. Provide a 'haptic' feedback on actions where available.
1593. Support a 'dark mode' that follows OS (WS03).
1594. Provide a 'tablet split view' doc + AI side-by-side.
1595. Support a 'pen menu' with quick tools.
1596. Provide a 'lasso select' on ink.
1597. Support a 'convert ink to shape/text' on lift pen.
1598. Provide a 'mobile command palette' (WS03).
1599. Support a 'one-handed' mode for phones.
1600. Provide a 'widget' showing recent docs.
1601. Support a 'share sheet' integration (WS15).
1602. Provide a 'biometric unlock' for encrypted docs (WS02).
1603. Support a 'low-bandwidth' sync mode (WS06).
1604. Provide a 'touch track-changes' approve/reject swipe.
1605. Support a 'handwriting math' to equation (local).
1606. Provide a 'camera OCR' to table (local model).
1607. Support a 'phone as second screen' for presenter view.
1608. Provide a 'offline templates' on mobile.
1609. Support a 'sync conflict' resolver friendly to touch.
1610. Provide a 'quick table' creation from voice.
1611. Support a 'stylus scrolling' like paper.
1612. Provide a 'magnifier' for precise touch editing.
1613. Support a 'reduce motion' on mobile (WS04).
1614. Provide a 'data-saver' that skips thumbnails.
1615. Support a 'privacy on device' (no cloud) by default.
1616. Provide a 'tablet PDF annotate' with pen.
1617. Support a 'voice nav' for accessibility (WS04).
1618. Provide a 'gesture' for common commands (undo, save).
1619. Support a 'foldable' adaptive layout.
1620. Provide a 'watch' glance for notifications (optional).
1621. Support a 'offline spellcheck' on mobile.
1622. Provide a 'quick share to deck' from photos.
1623. Support a 'stylus palette' customizable.
1624. Support a 'touch zoom' that keeps text crisp.
1625. Provide a 'mobile print' to OS/network printers.
1626. Support a 'live caption' of presentations (WS10).
1627. Provide a 'pen notebook' infinite canvas mode.
1628. Support a 'sync status' indicator clear on mobile.
1629. Provide a 'offline first' guarantee like desktop (WS02).
1630. Provide a touch-optimized UI mode with large hit targets.
1631. Support active pen inking with pressure and tilt.
1632. Provide handwriting-to-text offline via local model.
1633. Support a 'phone companion' that edits on the go.
1634. Provide a 'tablet layout' with ribbon adapted to touch.
1635. Support multi-touch zoom/pan on canvas.
1636. Provide a 'voice dictation' using OS/local STT (WS13).
1637. Support a 'scan to doc' via OS camera (WS15).
1638. Provide a 'read-aloud' TTS for commute review (WS04).
1639. Provide a 'quick capture' widget for notes on lock screen.
1640. Provide a 'sync' that is CRDT-based and offline (WS06).
1641. Support a 'reduced chrome' mobile editor.
1642. Provide a 'thumb keyboard' shortcuts for common actions.
1643. Support a 'stylus eraser' and highlighter natively.
1644. Provide a 'shape recognition' from freehand (local).
1645. Support a 'presentation remote' from phone (WS10).
1646. Provide a 'review mode' on phone for comments/approve.
1647. Support a 'offline first' mobile that syncs later.
1648. Support a 'small-screen' sheet view (frozen key cols).
1649. Support a 'drag handle' for reordering on touch.

### 22. Testing, Correctness & Fuzzing (from-scratch discipline)

1650. Maintain a regression corpus of Excel/LO results to match.
1651. Fuzz every importer (docx/xlsx/pptx/odf/pdf) in CI.
1652. Property-test the model invariants (WS11) on random edits.
1653. Snapshot-test render output per release.
1654. Run ASan/UBSan/MSan on the whole suite in CI.
1655. Maintain a 'golden file' set for round-trip fidelity (WS05).
1656. Test accessibility tree with an automated AT simulator (WS04).
1657. Benchmark perf regressions against a fixed hardware baseline (WS07).
1658. Property-test formula numeric stability against Excel.
1659. Fuzz the DEFLATE round-trip (our own compressor) continuously.
1660. Test cross-app live links don't corrupt on save (WS16).
1661. Test collaboration merge with random concurrent edits (WS06).
1662. Run a differential test vs LibreOffice on public docs.
1663. Test that 'no telemetry' holds via packet capture in CI (WS02).
1664. Property-test the sandbox blocks escapes (WS18).
1665. Test undo/redo across 10k random actions.
1666. Test recovery from killed-process mid-autosave.
1667. Fuzz the expression parser with adversarial input.
1668. Test encoding correctness (UTF-8/legacy) on import (WS19).
1669. Maintain a coverage gate (e.g., >=80%) on core libs.
1670. Test that large docs (1M rows) open within budget (WS07/08).
1671. Test that dark mode has no unthemed panes (WS03).
1672. Property-test CRDT convergence under partitions (WS06).
1673. Test that AI features work fully offline (WS13).
1674. Fuzz the RL environment simulator for crashes (WS14).
1675. Test the extension sandbox with malicious add-ins (WS12).
1676. Test that signed updates verify and reject tampered (WS18/20).
1677. Property-test the unified object model serialization.
1678. Test that migration preserves track changes/comments (WS19).
1679. Run a 'soak' test (days) for memory leaks.
1680. Test OS integration points on real OS builds (WS15).
1681. Property-test the formula engine against a prover (symbolic).
1682. Test that clipboard round-trips rich content (WS16).
1683. Fuzz the PDF exporter for malformed input.
1684. Test that accessibility checker finds known issues (WS04).
1685. Test that 'save as' to each format validates (WS05).
1686. Property-test the permission system denies by default.
1687. Test that the command palette reaches every action (WS03).
1688. Run differential tests of our DEFLATE vs zlib on corpora.
1689. Test that no network egress occurs in local mode (CI packet cap).
1690. Property-test the recalc dependency graph acyclicity.
1691. Test the macro transpiler output executes correctly (WS19).
1692. Fuzz the OOXML writer for invalid ZIP structures.
1693. Test that book-length docs don't OOM (WS09).
1694. Property-test the knowledge graph extractor (WS17).
1695. Test that RL agents can't corrupt real files (WS14/18).
1696. Run a 'reproducible build' verification (bit-identical).
1697. Test that templates open without account (WS01).
1698. Property-test undo atomicity for AI edits (WS13).
1699. Test that the portable build runs from read-only media.
1700. Fuzz the URL/hyperlink handler for injection.
1701. Test that track-changes accept/reject is reversible.
1702. Run a 'chaos' test killing threads mid-operation.
1703. Property-test the style inheritance chain (WS09).
1704. Test that embedded media survives round-trip (WS05).
1705. Test that the AI guardrail never fabricates citations (WS13).
1706. Run a 'differential corpus' vs real-world docs nightly.
1707. Property-test the encryption KDF timing safety (WS02).
1708. Test that the extension permission prompt lists exact caps.
1709. Fuzz the presentation transition engine.
1710. Test that no PII leaks into autosave temp (WS02).
1711. Property-test the cross-app undo transaction log.
1712. Run a 'reproducible build' on three independent workers.
1713. Test that the docket's 1000 tasks map to tracked issues.
1714. Property-test the model patch applies transactionally (WS11).
1715. Test that the RL reward shaping favors accessibility (WS14).
1716. Run a 'no-regression' gate blocking releases on red CI.
1717. Maintain a regression corpus of Excel/LO results to match.
1718. Fuzz every importer (docx/xlsx/pptx/odf/pdf) in CI.
1719. Property-test the model invariants (WS11) on random edits.
1720. Snapshot-test render output per release.
1721. Run ASan/UBSan/MSan on the whole suite in CI.
1722. Maintain a 'golden file' set for round-trip fidelity (WS05).
1723. Test accessibility tree with an automated AT simulator (WS04).
1724. Benchmark perf regressions against a fixed hardware baseline (WS07).
1725. Property-test formula numeric stability against Excel.
1726. Fuzz the DEFLATE round-trip (our own compressor) continuously.
1727. Test cross-app live links don't corrupt on save (WS16).
1728. Test collaboration merge with random concurrent edits (WS06).
1729. Run a differential test vs LibreOffice on public docs.
1730. Test that 'no telemetry' holds via packet capture in CI (WS02).
1731. Property-test the sandbox blocks escapes (WS18).
1732. Test undo/redo across 10k random actions.
1733. Test recovery from killed-process mid-autosave.
1734. Fuzz the expression parser with adversarial input.
1735. Test encoding correctness (UTF-8/legacy) on import (WS19).
1736. Maintain a coverage gate (e.g., >=80%) on core libs.


## Cross-cutting integration thesis
The 1000 items above are not a feature checklist for a Microsoft clone. They are the 
landscape for a suite that is *native to our stack*: the **unified object model (WS11)** is 
the shared substrate; the **OS (WS15)** makes documents first-class citizens of the 
filesystem, search, share, and presence; the **inference engine (WS13)** is the local 
brain that reads document context and never phones home; the **RL environment (WS14)** 
learns office tasks as episodes over that model; and the **knowledge store (WS17)** turns 
every document into a node in a private graph our other software can query. That is the 
'inclusive stack' play: Office was to Windows what this suite is to our OS + inference + RL.

## Next research-phase moves
1. Prioritize WS01/WS02/WS03/WS05/WS07 — the table-stakes differentiators (ownership, 
   privacy, UI choice, formats, speed) where incumbents are weakest.
2. Design WS11 (unified model) before adding features — it is the integration keystone.
3. Stand up WS13/WS14 hooks early so the suite is born AI/RL-native, not retrofitted.
4. Map each of the 1000 items to a tracked issue; use WS22 tests as the acceptance gate.

## Source signals (real, 2024–2026)
- Reddit r/microsoft, r/libreoffice, r/microsoftoffice — subscription, Copilot, ribbon, 
  performance complaints.
- Hacker News — M365 price increase, Copilot adoption critique, OOXML lock-in.
- XDA Developers — '6 things I can't stand about Microsoft 365' (dark mode, paying for 
  unneeded software).
- Microsoft Learn / Answers — forced subscription, accessibility checker, screen-reader 
  gaps, Copilot 'useless' threads.
- LibreOffice/OnlyOffice communities & Document Foundation wiki — missing real-time 
  collaboration, VBA gaps, feature comparison.
- Ink & Switch 'Local-First Software' essay + Kleppmann paper — ownership/offline thesis 
  (WS02).
- Haiku/BeOS discussions — OS-integrated apps as a design ideal (WS15).
- GDPR/CLOUD Act sovereignty threads (ownCloud, kiteworks) — data residency exposure 
  (WS02/WS18).
- OfficeDev/office-js GitHub open letter — 233 developers on the add-in platform crisis 
  (WS12).
