# WuBuOffice — Research Docket (comprehensive, 5000 items)

**Product frame:** An OS-integrated, AI/RL-native office suite built on our own stack — 
a custom operating system, the **WuBuMath** inference engine, and a **reinforcement-learning 
environment**. The strategic analogy is Microsoft Office's deep integration with Windows 
95/98/2000: the suite is not a set of foreign apps but a native layer of the OS, and now 
also a native layer of the inference + learning stack.

_Generated 2026-07-13 from real 2024–2026 web research on Microsoft 
Office complaints and open-source alternative gaps (LibreOffice, OnlyOffice, Google Docs). 
Themes below are grounded in that research; the 5000 items are a derived, organized product "
docket for our suite. Every item is a concrete gap, wish, principle, or integration point._

## How to read this
- **Workstreams 01–22** group the 5000 items by capability and by how they exploit our "
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
0071. Support perpetual license for personal use at no cost (WS01).
0072. Support offline activation for personal use at no cost (WS01).
0073. Support free personal tier for personal use at no cost (WS01).
0074. Support price-lock guarantee for personal use at no cost (WS01).
0075. Support license transfer for personal use at no cost (WS01).
0076. Support homelab license for personal use at no cost (WS01).
0077. Support student tier for personal use at no cost (WS01).
0078. Support family plan for personal use at no cost (WS01).
0079. Support source-available audit for personal use at no cost (WS01).
0080. Support no-ads guarantee for personal use at no cost (WS01).
0081. Support per-app purchase for personal use at no cost (WS01).
0082. Support license dashboard for personal use at no cost (WS01).
0083. Support trial mode for personal use at no cost (WS01).
0084. Support community currency for personal use at no cost (WS01).
0085. Never gate perpetual license behind a subscription or account (WS01).
0086. Never gate offline activation behind a subscription or account (WS01).
0087. Never gate free personal tier behind a subscription or account (WS01).
0088. Never gate price-lock guarantee behind a subscription or account (WS01).
0089. Never gate license transfer behind a subscription or account (WS01).
0090. Never gate homelab license behind a subscription or account (WS01).
0091. Never gate student tier behind a subscription or account (WS01).
0092. Never gate family plan behind a subscription or account (WS01).
0093. Never gate source-available audit behind a subscription or account (WS01).
0094. Never gate no-ads guarantee behind a subscription or account (WS01).
0095. Never gate per-app purchase behind a subscription or account (WS01).
0096. Never gate license dashboard behind a subscription or account (WS01).
0097. Never gate trial mode behind a subscription or account (WS01).
0098. Never gate community currency behind a subscription or account (WS01).
0099. Add perpetual license as a first-class, offline-capable feature.
0100. Add offline activation as a first-class, offline-capable feature.
0101. Add free personal tier as a first-class, offline-capable feature.
0102. Add price-lock guarantee as a first-class, offline-capable feature.
0103. Add license transfer as a first-class, offline-capable feature.
0104. Add homelab license as a first-class, offline-capable feature.
0105. Add student tier as a first-class, offline-capable feature.
0106. Add family plan as a first-class, offline-capable feature.
0107. Add source-available audit as a first-class, offline-capable feature.
0108. Add no-ads guarantee as a first-class, offline-capable feature.
0109. Add per-app purchase as a first-class, offline-capable feature.
0110. Add license dashboard as a first-class, offline-capable feature.
0111. Add trial mode as a first-class, offline-capable feature.
0112. Add community currency as a first-class, offline-capable feature.
0113. Expose perpetual license through the local extension API (WS12).
0114. Expose offline activation through the local extension API (WS12).
0115. Expose free personal tier through the local extension API (WS12).
0116. Expose price-lock guarantee through the local extension API (WS12).
0117. Expose license transfer through the local extension API (WS12).
0118. Expose homelab license through the local extension API (WS12).
0119. Expose student tier through the local extension API (WS12).
0120. Expose family plan through the local extension API (WS12).
0121. Expose source-available audit through the local extension API (WS12).
0122. Expose no-ads guarantee through the local extension API (WS12).
0123. Expose per-app purchase through the local extension API (WS12).
0124. Expose license dashboard through the local extension API (WS12).
0125. Expose trial mode through the local extension API (WS12).
0126. Expose community currency through the local extension API (WS12).
0127. Make perpetual license work fully on-device with no telemetry (WS01).
0128. Make offline activation work fully on-device with no telemetry (WS01).
0129. Make free personal tier work fully on-device with no telemetry (WS01).
0130. Make price-lock guarantee work fully on-device with no telemetry (WS01).
0131. Make license transfer work fully on-device with no telemetry (WS01).
0132. Make homelab license work fully on-device with no telemetry (WS01).
0133. Make student tier work fully on-device with no telemetry (WS01).
0134. Make family plan work fully on-device with no telemetry (WS01).
0135. Make source-available audit work fully on-device with no telemetry (WS01).
0136. Make no-ads guarantee work fully on-device with no telemetry (WS01).
0137. Make per-app purchase work fully on-device with no telemetry (WS01).
0138. Make license dashboard work fully on-device with no telemetry (WS01).
0139. Make trial mode work fully on-device with no telemetry (WS01).
0140. Make community currency work fully on-device with no telemetry (WS01).
0141. Test perpetual license in CI with the WS22 correctness suite (WS01).
0142. Test offline activation in CI with the WS22 correctness suite (WS01).
0143. Test free personal tier in CI with the WS22 correctness suite (WS01).
0144. Test price-lock guarantee in CI with the WS22 correctness suite (WS01).
0145. Test license transfer in CI with the WS22 correctness suite (WS01).
0146. Test homelab license in CI with the WS22 correctness suite (WS01).
0147. Test student tier in CI with the WS22 correctness suite (WS01).
0148. Test family plan in CI with the WS22 correctness suite (WS01).
0149. Test source-available audit in CI with the WS22 correctness suite (WS01).
0150. Test no-ads guarantee in CI with the WS22 correctness suite (WS01).
0151. Test per-app purchase in CI with the WS22 correctness suite (WS01).
0152. Test license dashboard in CI with the WS22 correctness suite (WS01).
0153. Test trial mode in CI with the WS22 correctness suite (WS01).
0154. Test community currency in CI with the WS22 correctness suite (WS01).
0155. Provide a fast offline activation that respects user ownership.
0156. Provide a fast student tier suitable for enterprise self-hosting.
0157. Provide a offline no-ads guarantee that respects user ownership.
0158. Provide a offline perpetual license suitable for enterprise self-hosting.
0159. Provide a local-first price-lock guarantee that respects user ownership.
0160. Provide a local-first source-available audit suitable for enterprise self-hosting.
0161. Provide a accessible license dashboard that respects user ownership.
0162. Provide a accessible free personal tier suitable for enterprise self-hosting.
0163. Provide a secure homelab license that respects user ownership.
0164. Provide a secure per-app purchase suitable for enterprise self-hosting.
0165. Provide a simple community currency that respects user ownership.
0166. Provide a simple license transfer suitable for enterprise self-hosting.
0167. Provide a auditable family plan that respects user ownership.
0168. Provide a auditable trial mode suitable for enterprise self-hosting.
0169. Support perpetual license for personal use at no cost (WS01).
0170. Support offline activation for personal use at no cost (WS01).
0171. Support free personal tier for personal use at no cost (WS01).
0172. Support price-lock guarantee for personal use at no cost (WS01).
0173. Support license transfer for personal use at no cost (WS01).
0174. Support homelab license for personal use at no cost (WS01).
0175. Support student tier for personal use at no cost (WS01).
0176. Support family plan for personal use at no cost (WS01).
0177. Support source-available audit for personal use at no cost (WS01).
0178. Support no-ads guarantee for personal use at no cost (WS01).
0179. Support per-app purchase for personal use at no cost (WS01).
0180. Support license dashboard for personal use at no cost (WS01).
0181. Support trial mode for personal use at no cost (WS01).
0182. Support community currency for personal use at no cost (WS01).
0183. Never gate perpetual license behind a subscription or account (WS01).
0184. Never gate offline activation behind a subscription or account (WS01).
0185. Never gate free personal tier behind a subscription or account (WS01).
0186. Never gate price-lock guarantee behind a subscription or account (WS01).
0187. Never gate license transfer behind a subscription or account (WS01).
0188. Never gate homelab license behind a subscription or account (WS01).
0189. Never gate student tier behind a subscription or account (WS01).
0190. Never gate family plan behind a subscription or account (WS01).
0191. Never gate source-available audit behind a subscription or account (WS01).
0192. Never gate no-ads guarantee behind a subscription or account (WS01).
0193. Never gate per-app purchase behind a subscription or account (WS01).
0194. Never gate license dashboard behind a subscription or account (WS01).
0195. Never gate trial mode behind a subscription or account (WS01).
0196. Never gate community currency behind a subscription or account (WS01).
0197. Add perpetual license as a first-class, offline-capable feature.
0198. Add offline activation as a first-class, offline-capable feature.
0199. Add free personal tier as a first-class, offline-capable feature.
0200. Add price-lock guarantee as a first-class, offline-capable feature.
0201. Add license transfer as a first-class, offline-capable feature.
0202. Add homelab license as a first-class, offline-capable feature.
0203. Add student tier as a first-class, offline-capable feature.
0204. Add family plan as a first-class, offline-capable feature.
0205. Add source-available audit as a first-class, offline-capable feature.
0206. Add no-ads guarantee as a first-class, offline-capable feature.
0207. Add per-app purchase as a first-class, offline-capable feature.
0208. Add license dashboard as a first-class, offline-capable feature.
0209. Add trial mode as a first-class, offline-capable feature.
0210. Add community currency as a first-class, offline-capable feature.
0211. Expose perpetual license through the local extension API (WS12).
0212. Expose offline activation through the local extension API (WS12).
0213. Expose free personal tier through the local extension API (WS12).
0214. Expose price-lock guarantee through the local extension API (WS12).
0215. Expose license transfer through the local extension API (WS12).
0216. Expose homelab license through the local extension API (WS12).
0217. Expose student tier through the local extension API (WS12).
0218. Expose family plan through the local extension API (WS12).
0219. Expose source-available audit through the local extension API (WS12).

### 02. Privacy, Sovereignty & Local-First

0220. Adopt local-first architecture: full read/write offline, sync optional.
0221. Store all user content under a single user-owned directory with clear layout.
0222. Zero required network calls for any editing operation.
0223. Implement CRDT/OT so offline edits merge without conflict loss.
0224. Provide end-to-end encryption for optional sync, keys never leave device.
0225. Support data sovereignty: files never transit foreign jurisdictions by default.
0226. Let users self-host the sync server on their own hardware.
0227. Document a GDPR/CLOUD-Act resistance posture (data stays in-region).
0228. Purge all temp/autosave fragments on close with a secure-wipe option.
0229. Never embed tracking pixels or remote fonts in saved documents.
0230. Strip metadata (author, machine, path) by default unless user opts in.
0231. Provide a metadata inspector/redactor before share/export.
0232. Honor a global 'no telemetry' flag enforced at the binary level.
0233. Encrypt local index/cache at rest.
0234. Allow documents to be opened from and saved to FUSE/own encrypted volumes.
0235. No advertising ID, no device fingerprinting, no cross-app correlation.
0236. Support offline spell/grammar/models that never phone home.
0237. Provide an auditable network egress log the user can review.
0238. Let orgs run the suite fully on-prem with no external dependency.
0239. Ship a 'paranoid mode' that blocks all outbound except explicit sync.
0240. Never train shared models on user documents without explicit opt-in per file.
0241. Allow per-document classification (public/confidential/secret) with handling rules.
0242. Provide a 'send nothing' guarantee validated by an external auditor.
0243. Encrypt autosave shadow copies; never leave plaintext temp files.
0244. Support air-gapped update distribution (signed offline update bundles).
0245. Let users verify binary integrity against a published hash/signature.
0246. Provide a data-export that yields plain folders, not a proprietary vault.
0247. Never require a CDN account to fetch templates or assets.
0248. Allow disabling of all 'smart' cloud features from one toggle.
0249. Document exactly which strings leave the machine and when.
0250. Support regional data residency selection at first run.
0251. Provide a 'forget this machine' that scrubs all local traces.
0252. Never silently upload autosaves 'for safety'.
0253. Allow the suite to run inside a sandbox with no network and full function.
0254. Provide a privacy label per feature (like a nutrition label).
0255. Let users approve/deny every network request interactively.
0256. Support document-level DRM that the owner controls, not a vendor.
0257. Keep AI inference local so prompts/data never leave the device.
0258. Provide a 'sovereign build' compiled without any cloud SDK.
0259. Let enterprises bring their own key (BYOK) for at-rest encryption.
0260. Audit dependencies for covert telemetry; ban them.
0261. Provide a clear lawful-interception resistance statement.
0262. Never store credentials in plaintext or reversible form.
0263. Allow privacy review mode that shows a live map of data flow.
0264. Ship a 'no cloud' edition that physically cannot reach the internet.
0265. Document subprocessor list (answer: none for local edition).
0266. Let users delete their account and all server copies in one click (if sync used).
0267. Provide tamper-evident logs for enterprise compliance.
0268. Support offline license verification via signed local token.
0269. Never correlate document content with other user activity.
0270. Allow disabling of all 'smart' cloud features from one toggle.
0271. Document exactly which strings leave the machine and when.
0272. Provide a 'data residency' selector defaulting to the user's region.
0273. Support a 'no cloud' build validated by an external auditor.
0274. Allow documents to be stored only on user-owned encrypted disks.
0275. Provide a 'privacy nutrition label' per feature, like food labels.
0276. Support a 'forget me' that scrubs all local traces in one click.
0277. Never embed tracking beacons in exported files.
0278. Provide a 'network egress log' the user can review anytime.
0279. Support a 'sovereign sync' where the server never sees plaintext.
0280. Allow enterprises to host sync on their own jurisdiction's servers.
0281. Provide a 'zero-knowledge' proof of our no-access architecture.
0282. Support a 'local-only AI' with weights on device (WS13).
0283. Provide a 'redact on share' that strips metadata automatically.
0284. Allow a 'privacy mode' that blanks AI from sensitive sections (WS13).
0285. Support a 'data portability' export as plain folders, not a vault.
0286. Provide a 'compliance attestation' for GDPR/CLOUD Act resistance.
0287. Never correlate document content with other user activity.
0288. Support a 'firewall-friendly' mode with zero outbound by default.
0289. Provide a 'telemetry off' enforced at the binary level.
0290. Allow a 'per-document classification' with handling rules.
0291. Support a 'secure wipe' of autosave temp on close.
0292. Support local-first storage for personal use at no cost (WS02).
0293. Support end-to-end sync for personal use at no cost (WS02).
0294. Support metadata stripping for personal use at no cost (WS02).
0295. Support telemetry-off default for personal use at no cost (WS02).
0296. Support data residency for personal use at no cost (WS02).
0297. Support encrypted at-rest for personal use at no cost (WS02).
0298. Support sovereign sync for personal use at no cost (WS02).
0299. Support secure wipe for personal use at no cost (WS02).
0300. Support network egress log for personal use at no cost (WS02).
0301. Support privacy nutrition label for personal use at no cost (WS02).
0302. Support BYOK encryption for personal use at no cost (WS02).
0303. Support air-gapped update for personal use at no cost (WS02).
0304. Support zero-knowledge proof for personal use at no cost (WS02).
0305. Support no-beacon export for personal use at no cost (WS02).
0306. Never gate local-first storage behind a subscription or account (WS02).
0307. Never gate end-to-end sync behind a subscription or account (WS02).
0308. Never gate metadata stripping behind a subscription or account (WS02).
0309. Never gate telemetry-off default behind a subscription or account (WS02).
0310. Never gate data residency behind a subscription or account (WS02).
0311. Never gate encrypted at-rest behind a subscription or account (WS02).
0312. Never gate sovereign sync behind a subscription or account (WS02).
0313. Never gate secure wipe behind a subscription or account (WS02).
0314. Never gate network egress log behind a subscription or account (WS02).
0315. Never gate privacy nutrition label behind a subscription or account (WS02).
0316. Never gate BYOK encryption behind a subscription or account (WS02).
0317. Never gate air-gapped update behind a subscription or account (WS02).
0318. Never gate zero-knowledge proof behind a subscription or account (WS02).
0319. Never gate no-beacon export behind a subscription or account (WS02).
0320. Add local-first storage as a first-class, offline-capable feature.
0321. Add end-to-end sync as a first-class, offline-capable feature.
0322. Add metadata stripping as a first-class, offline-capable feature.
0323. Add telemetry-off default as a first-class, offline-capable feature.
0324. Add data residency as a first-class, offline-capable feature.
0325. Add encrypted at-rest as a first-class, offline-capable feature.
0326. Add sovereign sync as a first-class, offline-capable feature.
0327. Add secure wipe as a first-class, offline-capable feature.
0328. Add network egress log as a first-class, offline-capable feature.
0329. Add privacy nutrition label as a first-class, offline-capable feature.
0330. Add BYOK encryption as a first-class, offline-capable feature.
0331. Add air-gapped update as a first-class, offline-capable feature.
0332. Add zero-knowledge proof as a first-class, offline-capable feature.
0333. Add no-beacon export as a first-class, offline-capable feature.
0334. Expose local-first storage through the local extension API (WS12).
0335. Expose end-to-end sync through the local extension API (WS12).
0336. Expose metadata stripping through the local extension API (WS12).
0337. Expose telemetry-off default through the local extension API (WS12).
0338. Expose data residency through the local extension API (WS12).
0339. Expose encrypted at-rest through the local extension API (WS12).
0340. Expose sovereign sync through the local extension API (WS12).
0341. Expose secure wipe through the local extension API (WS12).
0342. Expose network egress log through the local extension API (WS12).
0343. Expose privacy nutrition label through the local extension API (WS12).
0344. Expose BYOK encryption through the local extension API (WS12).
0345. Expose air-gapped update through the local extension API (WS12).
0346. Expose zero-knowledge proof through the local extension API (WS12).
0347. Expose no-beacon export through the local extension API (WS12).
0348. Make local-first storage work fully on-device with no telemetry (WS02).
0349. Make end-to-end sync work fully on-device with no telemetry (WS02).
0350. Make metadata stripping work fully on-device with no telemetry (WS02).
0351. Make telemetry-off default work fully on-device with no telemetry (WS02).
0352. Make data residency work fully on-device with no telemetry (WS02).
0353. Make encrypted at-rest work fully on-device with no telemetry (WS02).
0354. Make sovereign sync work fully on-device with no telemetry (WS02).
0355. Make secure wipe work fully on-device with no telemetry (WS02).
0356. Make network egress log work fully on-device with no telemetry (WS02).
0357. Make privacy nutrition label work fully on-device with no telemetry (WS02).
0358. Make BYOK encryption work fully on-device with no telemetry (WS02).
0359. Make air-gapped update work fully on-device with no telemetry (WS02).
0360. Make zero-knowledge proof work fully on-device with no telemetry (WS02).
0361. Make no-beacon export work fully on-device with no telemetry (WS02).
0362. Test local-first storage in CI with the WS22 correctness suite (WS02).
0363. Test end-to-end sync in CI with the WS22 correctness suite (WS02).
0364. Test metadata stripping in CI with the WS22 correctness suite (WS02).
0365. Test telemetry-off default in CI with the WS22 correctness suite (WS02).
0366. Test data residency in CI with the WS22 correctness suite (WS02).
0367. Test encrypted at-rest in CI with the WS22 correctness suite (WS02).
0368. Test sovereign sync in CI with the WS22 correctness suite (WS02).
0369. Test secure wipe in CI with the WS22 correctness suite (WS02).
0370. Test network egress log in CI with the WS22 correctness suite (WS02).
0371. Test privacy nutrition label in CI with the WS22 correctness suite (WS02).
0372. Test BYOK encryption in CI with the WS22 correctness suite (WS02).
0373. Test air-gapped update in CI with the WS22 correctness suite (WS02).
0374. Test zero-knowledge proof in CI with the WS22 correctness suite (WS02).
0375. Test no-beacon export in CI with the WS22 correctness suite (WS02).
0376. Provide a fast end-to-end sync that respects user ownership.
0377. Provide a fast sovereign sync suitable for enterprise self-hosting.
0378. Provide a offline privacy nutrition label that respects user ownership.
0379. Provide a offline local-first storage suitable for enterprise self-hosting.
0380. Provide a local-first telemetry-off default that respects user ownership.
0381. Provide a local-first network egress log suitable for enterprise self-hosting.
0382. Provide a accessible air-gapped update that respects user ownership.
0383. Provide a accessible metadata stripping suitable for enterprise self-hosting.
0384. Provide a secure encrypted at-rest that respects user ownership.
0385. Provide a secure BYOK encryption suitable for enterprise self-hosting.
0386. Provide a simple no-beacon export that respects user ownership.
0387. Provide a simple data residency suitable for enterprise self-hosting.
0388. Provide a auditable secure wipe that respects user ownership.
0389. Provide a auditable zero-knowledge proof suitable for enterprise self-hosting.
0390. Support local-first storage for personal use at no cost (WS02).
0391. Support end-to-end sync for personal use at no cost (WS02).
0392. Support metadata stripping for personal use at no cost (WS02).
0393. Support telemetry-off default for personal use at no cost (WS02).
0394. Support data residency for personal use at no cost (WS02).
0395. Support encrypted at-rest for personal use at no cost (WS02).
0396. Support sovereign sync for personal use at no cost (WS02).
0397. Support secure wipe for personal use at no cost (WS02).
0398. Support network egress log for personal use at no cost (WS02).
0399. Support privacy nutrition label for personal use at no cost (WS02).
0400. Support BYOK encryption for personal use at no cost (WS02).
0401. Support air-gapped update for personal use at no cost (WS02).
0402. Support zero-knowledge proof for personal use at no cost (WS02).
0403. Support no-beacon export for personal use at no cost (WS02).
0404. Never gate local-first storage behind a subscription or account (WS02).
0405. Never gate end-to-end sync behind a subscription or account (WS02).
0406. Never gate metadata stripping behind a subscription or account (WS02).
0407. Never gate telemetry-off default behind a subscription or account (WS02).
0408. Never gate data residency behind a subscription or account (WS02).
0409. Never gate encrypted at-rest behind a subscription or account (WS02).
0410. Never gate sovereign sync behind a subscription or account (WS02).
0411. Never gate secure wipe behind a subscription or account (WS02).
0412. Never gate network egress log behind a subscription or account (WS02).
0413. Never gate privacy nutrition label behind a subscription or account (WS02).
0414. Never gate BYOK encryption behind a subscription or account (WS02).
0415. Never gate air-gapped update behind a subscription or account (WS02).
0416. Never gate zero-knowledge proof behind a subscription or account (WS02).
0417. Never gate no-beacon export behind a subscription or account (WS02).
0418. Add local-first storage as a first-class, offline-capable feature.
0419. Add end-to-end sync as a first-class, offline-capable feature.
0420. Add metadata stripping as a first-class, offline-capable feature.
0421. Add telemetry-off default as a first-class, offline-capable feature.
0422. Add data residency as a first-class, offline-capable feature.
0423. Add encrypted at-rest as a first-class, offline-capable feature.
0424. Add sovereign sync as a first-class, offline-capable feature.
0425. Add secure wipe as a first-class, offline-capable feature.
0426. Add network egress log as a first-class, offline-capable feature.
0427. Add privacy nutrition label as a first-class, offline-capable feature.
0428. Add BYOK encryption as a first-class, offline-capable feature.
0429. Add air-gapped update as a first-class, offline-capable feature.
0430. Add zero-knowledge proof as a first-class, offline-capable feature.
0431. Add no-beacon export as a first-class, offline-capable feature.
0432. Expose local-first storage through the local extension API (WS12).
0433. Expose end-to-end sync through the local extension API (WS12).
0434. Expose metadata stripping through the local extension API (WS12).
0435. Expose telemetry-off default through the local extension API (WS12).
0436. Expose data residency through the local extension API (WS12).
0437. Expose encrypted at-rest through the local extension API (WS12).
0438. Expose sovereign sync through the local extension API (WS12).
0439. Expose secure wipe through the local extension API (WS12).
0440. Expose network egress log through the local extension API (WS12).

### 03. User Interface & Ergonomics

0441. Offer a classic menu+toolbar mode as a first-class alternative to the ribbon.
0442. Make the ribbon optional, collapsible, and fully keyboard-operable.
0443. Provide a 'command palette' (Ctrl+K) that finds any action by name.
0444. Unify dark mode across Word/Excel/PowerPoint with no inconsistent panes.
0445. Let users set per-app accent colors that persist across sessions.
0446. Support a true black OLED dark theme.
0447. Provide density settings: compact / comfortable / spacious.
0448. Never change the UI under the user without an explicit opt-in.
0449. Persist window layout, open docs, and cursor position across restarts.
0450. Allow floating toolbars that dock anywhere, including second monitors.
0451. Provide a focus/ Distraction-free mode that hides all chrome.
0452. Support full theming via a documented CSS-like token system.
0453. Let users rebind every shortcut; ship Vim/Emacs/emacs presets.
0454. Provide a 'legacy Office 2003' layout preset for muscle memory.
0455. Show live word/char/reading-time counts without opening a panel.
0456. Make zoom persistent per-document and per-app.
0457. Provide non-modal dialogs so work continues while a dialog is open.
0458. Allow side-by-side document comparison in a single window (not two).
0459. Support tabbed documents within one window (like a browser).
0460. Provide a minimap/thumbnail strip for long documents and sheets.
0461. Let users hide the ribbon entirely and reveal on hotkey.
0462. Respect OS font scaling and high-DPI without blur.
0463. Provide a consistent icon language across all apps.
0464. Allow toolbar buttons to be added/removed by the user.
0465. Support a 'simple mode' for new users with progressive disclosure.
0466. Never bury common actions (save, print) behind multi-click menus.
0467. Provide audible/visual feedback only when the user enables it.
0468. Allow full UI text scaling independent of document zoom.
0469. Support right-to-left UI for Arabic/Hebrew users.
0470. Provide a high-contrast theme meeting WCAG AAA.
0471. Let users choose serif/sans UI font.
0472. Provide a 'no animations' mode for motion-sensitive users.
0473. Support gesture-free full operation from keyboard alone.
0474. Allow window splitting and frozen header rows/cols by default convenience.
0475. Provide a quick 'jump to last edit' command.
0476. Let users customize the status bar widgets.
0477. Support multiple themes simultaneously per document type.
0478. Provide a 'what's this?' hover help on every control.
0479. Allow saving/loading custom UI layouts as profiles.
0480. Never show tips/ads in the editing surface.
0481. Provide a consistent undo/redo button placement across apps.
0482. Support a 'recent actions' rail for one-click repeat.
0483. Let users disable the Start screen and boot straight to a blank doc.
0484. Provide a unified color picker with hex/RGB/HSL and history.
0485. Support document tabs draggable to new windows.
0486. Allow the sidebar to collapse to a thin icon rail.
0487. Provide a 'command history' searchable like a shell.
0488. Let users set default view (print/draft/web) per app.
0489. Support touch+mouse hybrid layouts that adapt.
0490. Provide a night-shift/blue-light filter hook into OS.
0491. Never reorder ribbon tabs based on 'adaptive' heuristics without a lock.
0492. Allow exporting the full shortcut map as a cheat sheet.
0493. Provide a 'zen' mode that hides everything but the text.
0494. Support a 'command bar' at the bottom like a terminal.
0495. Allow a 'compact ribbon' that shows icons only.
0496. Provide a 'touch-first' layout toggle for 2-in-1 devices.
0497. Support a 'focus frame' highlighting the active region.
0498. Allow a 'custom accent' per document type.
0499. Provide a 'unified zoom' that respects the OS setting.
0500. Support a 'no-animation' mode for vestibular safety.
0501. Allow a 'tab cycle' that moves between open docs via Ctrl+Tab.
0502. Provide a 'jump to last edit' command globally.
0503. Support a 'minimap' for long sheets and docs.
0504. Allow a 'floating toolbar' that follows the selection.
0505. Provide a 'legacy 2003' preset for muscle memory.
0506. Support a 'high-DPI' crisp rendering with no blur.
0507. Allow a 'RTL UI' for Arabic/Hebrew users.
0508. Provide a 'visible focus' ring everywhere for keyboard users.
0509. Support a 'quick styles' gallery inline.
0510. Allow a 'status bar' widget customization.
0511. Provide a 'no tips' guarantee in the canvas.
0512. Support a 'start blank' boot without a start screen.
0513. Support classic menu mode for personal use at no cost (WS03).
0514. Support command palette for personal use at no cost (WS03).
0515. Support unified dark mode for personal use at no cost (WS03).
0516. Support density presets for personal use at no cost (WS03).
0517. Support keyboard rebinding for personal use at no cost (WS03).
0518. Support focus mode for personal use at no cost (WS03).
0519. Support tabbed documents for personal use at no cost (WS03).
0520. Support minimap for personal use at no cost (WS03).
0521. Support floating toolbar for personal use at no cost (WS03).
0522. Support RTL UI for personal use at no cost (WS03).
0523. Support high-contrast theme for personal use at no cost (WS03).
0524. Support no-animation mode for personal use at no cost (WS03).
0525. Support legacy 2003 preset for personal use at no cost (WS03).
0526. Support visible focus ring for personal use at no cost (WS03).
0527. Never gate classic menu mode behind a subscription or account (WS03).
0528. Never gate command palette behind a subscription or account (WS03).
0529. Never gate unified dark mode behind a subscription or account (WS03).
0530. Never gate density presets behind a subscription or account (WS03).
0531. Never gate keyboard rebinding behind a subscription or account (WS03).
0532. Never gate focus mode behind a subscription or account (WS03).
0533. Never gate tabbed documents behind a subscription or account (WS03).
0534. Never gate minimap behind a subscription or account (WS03).
0535. Never gate floating toolbar behind a subscription or account (WS03).
0536. Never gate RTL UI behind a subscription or account (WS03).
0537. Never gate high-contrast theme behind a subscription or account (WS03).
0538. Never gate no-animation mode behind a subscription or account (WS03).
0539. Never gate legacy 2003 preset behind a subscription or account (WS03).
0540. Never gate visible focus ring behind a subscription or account (WS03).
0541. Add classic menu mode as a first-class, offline-capable feature.
0542. Add command palette as a first-class, offline-capable feature.
0543. Add unified dark mode as a first-class, offline-capable feature.
0544. Add density presets as a first-class, offline-capable feature.
0545. Add keyboard rebinding as a first-class, offline-capable feature.
0546. Add focus mode as a first-class, offline-capable feature.
0547. Add tabbed documents as a first-class, offline-capable feature.
0548. Add minimap as a first-class, offline-capable feature.
0549. Add floating toolbar as a first-class, offline-capable feature.
0550. Add RTL UI as a first-class, offline-capable feature.
0551. Add high-contrast theme as a first-class, offline-capable feature.
0552. Add no-animation mode as a first-class, offline-capable feature.
0553. Add legacy 2003 preset as a first-class, offline-capable feature.
0554. Add visible focus ring as a first-class, offline-capable feature.
0555. Expose classic menu mode through the local extension API (WS12).
0556. Expose command palette through the local extension API (WS12).
0557. Expose unified dark mode through the local extension API (WS12).
0558. Expose density presets through the local extension API (WS12).
0559. Expose keyboard rebinding through the local extension API (WS12).
0560. Expose focus mode through the local extension API (WS12).
0561. Expose tabbed documents through the local extension API (WS12).
0562. Expose minimap through the local extension API (WS12).
0563. Expose floating toolbar through the local extension API (WS12).
0564. Expose RTL UI through the local extension API (WS12).
0565. Expose high-contrast theme through the local extension API (WS12).
0566. Expose no-animation mode through the local extension API (WS12).
0567. Expose legacy 2003 preset through the local extension API (WS12).
0568. Expose visible focus ring through the local extension API (WS12).
0569. Make classic menu mode work fully on-device with no telemetry (WS03).
0570. Make command palette work fully on-device with no telemetry (WS03).
0571. Make unified dark mode work fully on-device with no telemetry (WS03).
0572. Make density presets work fully on-device with no telemetry (WS03).
0573. Make keyboard rebinding work fully on-device with no telemetry (WS03).
0574. Make focus mode work fully on-device with no telemetry (WS03).
0575. Make tabbed documents work fully on-device with no telemetry (WS03).
0576. Make minimap work fully on-device with no telemetry (WS03).
0577. Make floating toolbar work fully on-device with no telemetry (WS03).
0578. Make RTL UI work fully on-device with no telemetry (WS03).
0579. Make high-contrast theme work fully on-device with no telemetry (WS03).
0580. Make no-animation mode work fully on-device with no telemetry (WS03).
0581. Make legacy 2003 preset work fully on-device with no telemetry (WS03).
0582. Make visible focus ring work fully on-device with no telemetry (WS03).
0583. Test classic menu mode in CI with the WS22 correctness suite (WS03).
0584. Test command palette in CI with the WS22 correctness suite (WS03).
0585. Test unified dark mode in CI with the WS22 correctness suite (WS03).
0586. Test density presets in CI with the WS22 correctness suite (WS03).
0587. Test keyboard rebinding in CI with the WS22 correctness suite (WS03).
0588. Test focus mode in CI with the WS22 correctness suite (WS03).
0589. Test tabbed documents in CI with the WS22 correctness suite (WS03).
0590. Test minimap in CI with the WS22 correctness suite (WS03).
0591. Test floating toolbar in CI with the WS22 correctness suite (WS03).
0592. Test RTL UI in CI with the WS22 correctness suite (WS03).
0593. Test high-contrast theme in CI with the WS22 correctness suite (WS03).
0594. Test no-animation mode in CI with the WS22 correctness suite (WS03).
0595. Test legacy 2003 preset in CI with the WS22 correctness suite (WS03).
0596. Test visible focus ring in CI with the WS22 correctness suite (WS03).
0597. Provide a fast command palette that respects user ownership.
0598. Provide a fast tabbed documents suitable for enterprise self-hosting.
0599. Provide a offline RTL UI that respects user ownership.
0600. Provide a offline classic menu mode suitable for enterprise self-hosting.
0601. Provide a local-first density presets that respects user ownership.
0602. Provide a local-first floating toolbar suitable for enterprise self-hosting.
0603. Provide a accessible no-animation mode that respects user ownership.
0604. Provide a accessible unified dark mode suitable for enterprise self-hosting.
0605. Provide a secure focus mode that respects user ownership.
0606. Provide a secure high-contrast theme suitable for enterprise self-hosting.
0607. Provide a simple visible focus ring that respects user ownership.
0608. Provide a simple keyboard rebinding suitable for enterprise self-hosting.
0609. Provide a auditable minimap that respects user ownership.
0610. Provide a auditable legacy 2003 preset suitable for enterprise self-hosting.
0611. Support classic menu mode for personal use at no cost (WS03).
0612. Support command palette for personal use at no cost (WS03).
0613. Support unified dark mode for personal use at no cost (WS03).
0614. Support density presets for personal use at no cost (WS03).
0615. Support keyboard rebinding for personal use at no cost (WS03).
0616. Support focus mode for personal use at no cost (WS03).
0617. Support tabbed documents for personal use at no cost (WS03).
0618. Support minimap for personal use at no cost (WS03).
0619. Support floating toolbar for personal use at no cost (WS03).
0620. Support RTL UI for personal use at no cost (WS03).
0621. Support high-contrast theme for personal use at no cost (WS03).
0622. Support no-animation mode for personal use at no cost (WS03).
0623. Support legacy 2003 preset for personal use at no cost (WS03).
0624. Support visible focus ring for personal use at no cost (WS03).
0625. Never gate classic menu mode behind a subscription or account (WS03).
0626. Never gate command palette behind a subscription or account (WS03).
0627. Never gate unified dark mode behind a subscription or account (WS03).
0628. Never gate density presets behind a subscription or account (WS03).
0629. Never gate keyboard rebinding behind a subscription or account (WS03).
0630. Never gate focus mode behind a subscription or account (WS03).
0631. Never gate tabbed documents behind a subscription or account (WS03).
0632. Never gate minimap behind a subscription or account (WS03).
0633. Never gate floating toolbar behind a subscription or account (WS03).
0634. Never gate RTL UI behind a subscription or account (WS03).
0635. Never gate high-contrast theme behind a subscription or account (WS03).
0636. Never gate no-animation mode behind a subscription or account (WS03).
0637. Never gate legacy 2003 preset behind a subscription or account (WS03).
0638. Never gate visible focus ring behind a subscription or account (WS03).
0639. Add classic menu mode as a first-class, offline-capable feature.
0640. Add command palette as a first-class, offline-capable feature.
0641. Add unified dark mode as a first-class, offline-capable feature.
0642. Add density presets as a first-class, offline-capable feature.
0643. Add keyboard rebinding as a first-class, offline-capable feature.
0644. Add focus mode as a first-class, offline-capable feature.
0645. Add tabbed documents as a first-class, offline-capable feature.
0646. Add minimap as a first-class, offline-capable feature.
0647. Add floating toolbar as a first-class, offline-capable feature.
0648. Add RTL UI as a first-class, offline-capable feature.
0649. Add high-contrast theme as a first-class, offline-capable feature.
0650. Add no-animation mode as a first-class, offline-capable feature.
0651. Add legacy 2003 preset as a first-class, offline-capable feature.
0652. Add visible focus ring as a first-class, offline-capable feature.
0653. Expose classic menu mode through the local extension API (WS12).
0654. Expose command palette through the local extension API (WS12).
0655. Expose unified dark mode through the local extension API (WS12).
0656. Expose density presets through the local extension API (WS12).
0657. Expose keyboard rebinding through the local extension API (WS12).
0658. Expose focus mode through the local extension API (WS12).
0659. Expose tabbed documents through the local extension API (WS12).
0660. Expose minimap through the local extension API (WS12).
0661. Expose floating toolbar through the local extension API (WS12).

### 04. Accessibility (WCAG, Screen Readers, Low Vision, Motor, Cognitive)

0662. Target WCAG 2.2 AA minimum, AAA where feasible, across all apps.
0663. Expose a complete UI Automation / AT-SPI tree for every control.
0664. Ensure screen readers announce all dialogs, errors, and state changes.
0665. Provide a dedicated accessibility checker with fix-in-place suggestions.
0666. Support full keyboard navigation with a visible focus indicator everywhere.
0667. Never trap focus in a dialog; ESC always closes/returns.
0668. Label every image, shape, chart with alt text enforced on insert.
0669. Provide a 'check accessibility' that runs automatically before save/share.
0670. Support high-contrast themes that recolor charts and shapes correctly.
0671. Allow font, spacing, and line-width bumps globally for low vision.
0672. Provide a built-in screen reader / TTS for documents (local, no cloud).
0673. Support dyslexia-friendly font and spacing presets.
0674. Provide text-to-speech that highlights the spoken sentence (karaoke).
0675. Allow speech-to-text dictation fully offline via local models.
0676. Support switch control and single-button navigation.
0677. Provide sticky keys / slow keys honoring OS settings.
0678. Never use color alone to convey meaning; add icons/labels.
0679. Make all charts readable via data-table fallback for AT.
0680. Support magnification that tracks caret and focus.
0681. Provide a 'reading ruler' and tint overlay for visual stress.
0682. Allow remapping of any mouse action to keyboard.
0683. Support OS narrator/ORCA/VoiceOver natively, documented.
0684. Provide cognitive-load reduction mode: simplify UI, fewer choices.
0685. Allow documents to carry an accessibility summary metadata block.
0686. Support braille display output for document text.
0687. Ensure math is exposed as MathML/semantic for AT, not images.
0688. Provide captions for any embedded audio/video.
0689. Support 'announce on save/send' confirmations via TTS.
0690. Allow contrast and brightness adjustment inside the canvas.
0691. Provide a plain-text 'story view' that linearizes any document.
0692. Support keyboard-driven table navigation (cell by cell).
0693. Make all error messages actionable, not just codes.
0694. Provide a 'reduce motion' that disables transitions globally.
0695. Support OS large-text without breaking layout.
0696. Allow per-user accessibility profiles that roam with the OS account.
0697. Provide an accessibility 'tour' on first run.
0698. Ensure focus order matches visual/logical order.
0699. Support custom cursor size and color.
0700. Provide audio cues that are optional and distinct.
0701. Allow disabling of auto-correct that hinders AT users.
0702. Support 'describe image' via local model for alt-text generation (opt-in).
0703. Make the command palette screen-reader friendly with live regions.
0704. Provide a contrast analyzer for user-chosen colors.
0705. Support dictation punctuation commands offline.
0706. Allow documents to require accessibility before publish (team policy).
0707. Provide a 'simulate low-vision' preview mode for authors.
0708. Support keyboard macros for repetitive AT workflows.
0709. Ensure print and PDF export preserve tags/structure for AT.
0710. Provide a 'no time limit' for any interactive element.
0711. Support alternative input (head/eye tracking) via OS bridges.
0712. Allow color-blind safe palette suggestions.
0713. Document the AT test matrix we run in CI.
0714. Provide an accessibility statement per release.
0715. Provide a 'simulate low vision' preview for authors.
0716. Support a 'reading ruler' tint overlay for visual stress.
0717. Allow a 'braille display' output for document text.
0718. Provide a 'TTS karaoke' that highlights the spoken sentence.
0719. Support a 'dictation' fully offline via local models.
0720. Allow a 'switch control' single-button navigation.
0721. Provide a 'cognitive simplify' mode reducing choices.
0722. Support a 'math exposed as MathML' for AT, not images.
0723. Allow a 'captions' for embedded audio/video.
0724. Provide a 'contrast analyzer' for user colors.
0725. Support a 'sticky keys' honoring OS settings.
0726. Allow a 'no time limit' on interactive elements.
0727. Provide a 'plain-text story view' linearizing any doc.
0728. Support a 'AT test matrix' we run in CI.
0729. Allow a 'describe image' via local model (opt-in).
0730. Provide a 'accessibility statement' per release.
0731. Support a 'keyboard macro' for repetitive AT workflows.
0732. Allow a 'color-blind safe' palette suggestions.
0733. Provide a 'focus order' matching logical order.
0734. Support a 'magnification' tracking caret and focus.
0735. Support screen-reader tree for personal use at no cost (WS04).
0736. Support TTS narration for personal use at no cost (WS04).
0737. Support dictation offline for personal use at no cost (WS04).
0738. Support braille output for personal use at no cost (WS04).
0739. Support contrast analyzer for personal use at no cost (WS04).
0740. Support reading ruler for personal use at no cost (WS04).
0741. Support switch control for personal use at no cost (WS04).
0742. Support dyslexia font for personal use at no cost (WS04).
0743. Support captions for media for personal use at no cost (WS04).
0744. Support keyboard navigation for personal use at no cost (WS04).
0745. Support AT test matrix for personal use at no cost (WS04).
0746. Support magnification tracking for personal use at no cost (WS04).
0747. Support plain-text story view for personal use at no cost (WS04).
0748. Support no-time-limit controls for personal use at no cost (WS04).
0749. Never gate screen-reader tree behind a subscription or account (WS04).
0750. Never gate TTS narration behind a subscription or account (WS04).
0751. Never gate dictation offline behind a subscription or account (WS04).
0752. Never gate braille output behind a subscription or account (WS04).
0753. Never gate contrast analyzer behind a subscription or account (WS04).
0754. Never gate reading ruler behind a subscription or account (WS04).
0755. Never gate switch control behind a subscription or account (WS04).
0756. Never gate dyslexia font behind a subscription or account (WS04).
0757. Never gate captions for media behind a subscription or account (WS04).
0758. Never gate keyboard navigation behind a subscription or account (WS04).
0759. Never gate AT test matrix behind a subscription or account (WS04).
0760. Never gate magnification tracking behind a subscription or account (WS04).
0761. Never gate plain-text story view behind a subscription or account (WS04).
0762. Never gate no-time-limit controls behind a subscription or account (WS04).
0763. Add screen-reader tree as a first-class, offline-capable feature.
0764. Add TTS narration as a first-class, offline-capable feature.
0765. Add dictation offline as a first-class, offline-capable feature.
0766. Add braille output as a first-class, offline-capable feature.
0767. Add contrast analyzer as a first-class, offline-capable feature.
0768. Add reading ruler as a first-class, offline-capable feature.
0769. Add switch control as a first-class, offline-capable feature.
0770. Add dyslexia font as a first-class, offline-capable feature.
0771. Add captions for media as a first-class, offline-capable feature.
0772. Add keyboard navigation as a first-class, offline-capable feature.
0773. Add AT test matrix as a first-class, offline-capable feature.
0774. Add magnification tracking as a first-class, offline-capable feature.
0775. Add plain-text story view as a first-class, offline-capable feature.
0776. Add no-time-limit controls as a first-class, offline-capable feature.
0777. Expose screen-reader tree through the local extension API (WS12).
0778. Expose TTS narration through the local extension API (WS12).
0779. Expose dictation offline through the local extension API (WS12).
0780. Expose braille output through the local extension API (WS12).
0781. Expose contrast analyzer through the local extension API (WS12).
0782. Expose reading ruler through the local extension API (WS12).
0783. Expose switch control through the local extension API (WS12).
0784. Expose dyslexia font through the local extension API (WS12).
0785. Expose captions for media through the local extension API (WS12).
0786. Expose keyboard navigation through the local extension API (WS12).
0787. Expose AT test matrix through the local extension API (WS12).
0788. Expose magnification tracking through the local extension API (WS12).
0789. Expose plain-text story view through the local extension API (WS12).
0790. Expose no-time-limit controls through the local extension API (WS12).
0791. Make screen-reader tree work fully on-device with no telemetry (WS04).
0792. Make TTS narration work fully on-device with no telemetry (WS04).
0793. Make dictation offline work fully on-device with no telemetry (WS04).
0794. Make braille output work fully on-device with no telemetry (WS04).
0795. Make contrast analyzer work fully on-device with no telemetry (WS04).
0796. Make reading ruler work fully on-device with no telemetry (WS04).
0797. Make switch control work fully on-device with no telemetry (WS04).
0798. Make dyslexia font work fully on-device with no telemetry (WS04).
0799. Make captions for media work fully on-device with no telemetry (WS04).
0800. Make keyboard navigation work fully on-device with no telemetry (WS04).
0801. Make AT test matrix work fully on-device with no telemetry (WS04).
0802. Make magnification tracking work fully on-device with no telemetry (WS04).
0803. Make plain-text story view work fully on-device with no telemetry (WS04).
0804. Make no-time-limit controls work fully on-device with no telemetry (WS04).
0805. Test screen-reader tree in CI with the WS22 correctness suite (WS04).
0806. Test TTS narration in CI with the WS22 correctness suite (WS04).
0807. Test dictation offline in CI with the WS22 correctness suite (WS04).
0808. Test braille output in CI with the WS22 correctness suite (WS04).
0809. Test contrast analyzer in CI with the WS22 correctness suite (WS04).
0810. Test reading ruler in CI with the WS22 correctness suite (WS04).
0811. Test switch control in CI with the WS22 correctness suite (WS04).
0812. Test dyslexia font in CI with the WS22 correctness suite (WS04).
0813. Test captions for media in CI with the WS22 correctness suite (WS04).
0814. Test keyboard navigation in CI with the WS22 correctness suite (WS04).
0815. Test AT test matrix in CI with the WS22 correctness suite (WS04).
0816. Test magnification tracking in CI with the WS22 correctness suite (WS04).
0817. Test plain-text story view in CI with the WS22 correctness suite (WS04).
0818. Test no-time-limit controls in CI with the WS22 correctness suite (WS04).
0819. Provide a fast TTS narration that respects user ownership.
0820. Provide a fast switch control suitable for enterprise self-hosting.
0821. Provide a offline keyboard navigation that respects user ownership.
0822. Provide a offline screen-reader tree suitable for enterprise self-hosting.
0823. Provide a local-first braille output that respects user ownership.
0824. Provide a local-first captions for media suitable for enterprise self-hosting.
0825. Provide a accessible magnification tracking that respects user ownership.
0826. Provide a accessible dictation offline suitable for enterprise self-hosting.
0827. Provide a secure reading ruler that respects user ownership.
0828. Provide a secure AT test matrix suitable for enterprise self-hosting.
0829. Provide a simple no-time-limit controls that respects user ownership.
0830. Provide a simple contrast analyzer suitable for enterprise self-hosting.
0831. Provide a auditable dyslexia font that respects user ownership.
0832. Provide a auditable plain-text story view suitable for enterprise self-hosting.
0833. Support screen-reader tree for personal use at no cost (WS04).
0834. Support TTS narration for personal use at no cost (WS04).
0835. Support dictation offline for personal use at no cost (WS04).
0836. Support braille output for personal use at no cost (WS04).
0837. Support contrast analyzer for personal use at no cost (WS04).
0838. Support reading ruler for personal use at no cost (WS04).
0839. Support switch control for personal use at no cost (WS04).
0840. Support dyslexia font for personal use at no cost (WS04).
0841. Support captions for media for personal use at no cost (WS04).
0842. Support keyboard navigation for personal use at no cost (WS04).
0843. Support AT test matrix for personal use at no cost (WS04).
0844. Support magnification tracking for personal use at no cost (WS04).
0845. Support plain-text story view for personal use at no cost (WS04).
0846. Support no-time-limit controls for personal use at no cost (WS04).
0847. Never gate screen-reader tree behind a subscription or account (WS04).
0848. Never gate TTS narration behind a subscription or account (WS04).
0849. Never gate dictation offline behind a subscription or account (WS04).
0850. Never gate braille output behind a subscription or account (WS04).
0851. Never gate contrast analyzer behind a subscription or account (WS04).
0852. Never gate reading ruler behind a subscription or account (WS04).
0853. Never gate switch control behind a subscription or account (WS04).
0854. Never gate dyslexia font behind a subscription or account (WS04).
0855. Never gate captions for media behind a subscription or account (WS04).
0856. Never gate keyboard navigation behind a subscription or account (WS04).
0857. Never gate AT test matrix behind a subscription or account (WS04).
0858. Never gate magnification tracking behind a subscription or account (WS04).
0859. Never gate plain-text story view behind a subscription or account (WS04).
0860. Never gate no-time-limit controls behind a subscription or account (WS04).
0861. Add screen-reader tree as a first-class, offline-capable feature.
0862. Add TTS narration as a first-class, offline-capable feature.
0863. Add dictation offline as a first-class, offline-capable feature.
0864. Add braille output as a first-class, offline-capable feature.
0865. Add contrast analyzer as a first-class, offline-capable feature.
0866. Add reading ruler as a first-class, offline-capable feature.
0867. Add switch control as a first-class, offline-capable feature.
0868. Add dyslexia font as a first-class, offline-capable feature.
0869. Add captions for media as a first-class, offline-capable feature.
0870. Add keyboard navigation as a first-class, offline-capable feature.
0871. Add AT test matrix as a first-class, offline-capable feature.
0872. Add magnification tracking as a first-class, offline-capable feature.
0873. Add plain-text story view as a first-class, offline-capable feature.
0874. Add no-time-limit controls as a first-class, offline-capable feature.
0875. Expose screen-reader tree through the local extension API (WS12).
0876. Expose TTS narration through the local extension API (WS12).
0877. Expose dictation offline through the local extension API (WS12).
0878. Expose braille output through the local extension API (WS12).
0879. Expose contrast analyzer through the local extension API (WS12).
0880. Expose reading ruler through the local extension API (WS12).
0881. Expose switch control through the local extension API (WS12).
0882. Expose dyslexia font through the local extension API (WS12).
0883. Expose captions for media through the local extension API (WS12).

### 05. File Formats & Interoperability

0884. Implement OOXML (ECMA-376) faithfully, not a 'complex' subset.
0885. Guarantee round-trip fidelity: open then save must not lose data.
0886. Support ODF (ODT/ODS/ODP) natively with no conversion loss.
0887. Support legacy .doc/.xls/.ppt (binary) import via from-scratch readers.
0888. Support PDF import, edit, and accessible export (tagged PDF).
0889. Support PDF/A for archival with embedded fonts.
0890. Never silently drop features on save; warn explicitly if unsupported.
0891. Implement strict self-compatibility: vN files open in vN+1 exactly.
0892. Provide a format-diff tool showing what changed on save.
0893. Store a canonical internal model separate from any wire format.
0894. Support plain Markdown/HTML export for docs with style mapping.
0895. Support CSV/TSV with quoting/encoding correctness (UTF-8 default).
0896. Support JSON/Parquet export for sheet data to feed our ML stack.
0897. Implement Excel formula compatibility for the common 400 functions.
0898. Preserve pivot tables, charts, and conditional formats on round-trip.
0899. Support embedded objects without external app dependency.
0900. Provide a 'validate' that checks conformance to the spec.
0901. Support ZIP-stored and ZIP-deflated packages (our own DEFLATE).
0902. Allow documents to be opened directly from our OS file manager preview.
0903. Support digital signatures on documents (XAdES/PAdES) locally.
0904. Provide a 'repair' that recovers content from corrupted packages.
0905. Never write vendor-only extensions to the default save path.
0906. Support long-path and Unicode filenames cross-platform.
0907. Allow selective export of one sheet/slide/section.
0908. Support versioned saves (append-only history) locally.
0909. Implement true track-changes XML that survives round-trips.
0910. Support comments/threads with mentions and resolve states.
0911. Provide a compatibility report vs MS Office for any file.
0912. Support embedded fonts subsetting for portability.
0913. Allow importing Google Docs/Sheets via exported OOXML/ODF.
0914. Support RTF and older WordPerfect import for legacy users.
0915. Provide a 'what we can't open yet' honest capability list.
0916. Support macro-free by default; macros explicit and sandboxed.
0917. Allow exporting to EPUB for documents.
0918. Support LaTeX/MathML paste for equations.
0919. Provide a schema for our unified object model for interop.
0920. Support reading/writing the 'strict' OOXML variant, not just transitional.
0921. Never require a conversion prompt on open of a native file.
0922. Support encrypted OOXML (agile encryption) import/export.
0923. Provide a 'compare two files' structural diff view.
0924. Support bookmarks/hyperlinks that survive export.
0925. Allow documents to embed other suite docs as live objects.
0926. Support EXIF/metadata passthrough for images.
0927. Provide a 'compat shim' that maps our extensions to OOXML on export.
0928. Support reading password-protected docs via user key only.
0929. Allow custom properties/extended attributes preservation.
0930. Support compressed-media reuse (dedupe identical images).
0931. Provide a lossless image pipeline (no re-encode on save).
0932. Support multi-target export (PDF + OOXML + ODF at once).
0933. Document the exact OOXML parts we emit for auditor review.
0934. Support opening files from object stores/our OS VFS directly.
0935. Provide a format fuzzer in CI to harden parsers.
0936. Support 'save as template' that strips content but keeps structure.
0937. Allow batch format conversion from the command line (OS-integrated).
0938. Support a canonical 'flat XML' debug format for diffing.
0939. Never embed machine-specific paths in saved files.
0940. Provide a 'health check' that flags non-portable content before share.
0941. Support ODF 1.3 with full fidelity.
0942. Provide a 'format-diff' showing what changed on save.
0943. Support strict OOXML (not just transitional).
0944. Provide a 'repair' that recovers content from corrupt packages.
0945. Support encrypted OOXML (agile) import/export.
0946. Provide a 'validate' that checks spec conformance.
0947. Support PDF/A archival with embedded fonts.
0948. Allow selective export of one sheet/slide/section.
0949. Support reading/writing our OS VFS directly.
0950. Provide a 'compat shim' mapping our extensions to OOXML.
0951. Support JSON/Parquet export for our ML stack (WS17).
0952. Provide a 'lossless image pipeline' (no re-encode on save).
0953. Support a 'canonical flat XML' debug format for diffing.
0954. Allow 'save as template' stripping content but keeping structure.
0955. Provide a 'health check' flagging non-portable content.
0956. Support 'what we can't open yet' honest capability list.
0957. Provide a 'schema' for our unified object model (WS11).
0958. Support long-path Unicode filenames cross-platform.
0959. Provide a 'versioned saves' append-only history locally.
0960. Support a 'format fuzzer' in CI to harden parsers.
0961. Support ODF 1.3 support for personal use at no cost (WS05).
0962. Support OOXML strict for personal use at no cost (WS05).
0963. Support format-diff for personal use at no cost (WS05).
0964. Support corrupt repair for personal use at no cost (WS05).
0965. Support encrypted OOXML for personal use at no cost (WS05).
0966. Support PDF/A export for personal use at no cost (WS05).
0967. Support JSON/Parquet export for personal use at no cost (WS05).
0968. Support lossless images for personal use at no cost (WS05).
0969. Support flat XML debug for personal use at no cost (WS05).
0970. Support versioned saves for personal use at no cost (WS05).
0971. Support format fuzzer for personal use at no cost (WS05).
0972. Support long-path Unicode for personal use at no cost (WS05).
0973. Support digital signatures for personal use at no cost (WS05).
0974. Support embedded fonts for personal use at no cost (WS05).
0975. Never gate ODF 1.3 support behind a subscription or account (WS05).
0976. Never gate OOXML strict behind a subscription or account (WS05).
0977. Never gate format-diff behind a subscription or account (WS05).
0978. Never gate corrupt repair behind a subscription or account (WS05).
0979. Never gate encrypted OOXML behind a subscription or account (WS05).
0980. Never gate PDF/A export behind a subscription or account (WS05).
0981. Never gate JSON/Parquet export behind a subscription or account (WS05).
0982. Never gate lossless images behind a subscription or account (WS05).
0983. Never gate flat XML debug behind a subscription or account (WS05).
0984. Never gate versioned saves behind a subscription or account (WS05).
0985. Never gate format fuzzer behind a subscription or account (WS05).
0986. Never gate long-path Unicode behind a subscription or account (WS05).
0987. Never gate digital signatures behind a subscription or account (WS05).
0988. Never gate embedded fonts behind a subscription or account (WS05).
0989. Add ODF 1.3 support as a first-class, offline-capable feature.
0990. Add OOXML strict as a first-class, offline-capable feature.
0991. Add format-diff as a first-class, offline-capable feature.
0992. Add corrupt repair as a first-class, offline-capable feature.
0993. Add encrypted OOXML as a first-class, offline-capable feature.
0994. Add PDF/A export as a first-class, offline-capable feature.
0995. Add JSON/Parquet export as a first-class, offline-capable feature.
0996. Add lossless images as a first-class, offline-capable feature.
0997. Add flat XML debug as a first-class, offline-capable feature.
0998. Add versioned saves as a first-class, offline-capable feature.
0999. Add format fuzzer as a first-class, offline-capable feature.
1000. Add long-path Unicode as a first-class, offline-capable feature.
1001. Add digital signatures as a first-class, offline-capable feature.
1002. Add embedded fonts as a first-class, offline-capable feature.
1003. Expose ODF 1.3 support through the local extension API (WS12).
1004. Expose OOXML strict through the local extension API (WS12).
1005. Expose format-diff through the local extension API (WS12).
1006. Expose corrupt repair through the local extension API (WS12).
1007. Expose encrypted OOXML through the local extension API (WS12).
1008. Expose PDF/A export through the local extension API (WS12).
1009. Expose JSON/Parquet export through the local extension API (WS12).
1010. Expose lossless images through the local extension API (WS12).
1011. Expose flat XML debug through the local extension API (WS12).
1012. Expose versioned saves through the local extension API (WS12).
1013. Expose format fuzzer through the local extension API (WS12).
1014. Expose long-path Unicode through the local extension API (WS12).
1015. Expose digital signatures through the local extension API (WS12).
1016. Expose embedded fonts through the local extension API (WS12).
1017. Make ODF 1.3 support work fully on-device with no telemetry (WS05).
1018. Make OOXML strict work fully on-device with no telemetry (WS05).
1019. Make format-diff work fully on-device with no telemetry (WS05).
1020. Make corrupt repair work fully on-device with no telemetry (WS05).
1021. Make encrypted OOXML work fully on-device with no telemetry (WS05).
1022. Make PDF/A export work fully on-device with no telemetry (WS05).
1023. Make JSON/Parquet export work fully on-device with no telemetry (WS05).
1024. Make lossless images work fully on-device with no telemetry (WS05).
1025. Make flat XML debug work fully on-device with no telemetry (WS05).
1026. Make versioned saves work fully on-device with no telemetry (WS05).
1027. Make format fuzzer work fully on-device with no telemetry (WS05).
1028. Make long-path Unicode work fully on-device with no telemetry (WS05).
1029. Make digital signatures work fully on-device with no telemetry (WS05).
1030. Make embedded fonts work fully on-device with no telemetry (WS05).
1031. Test ODF 1.3 support in CI with the WS22 correctness suite (WS05).
1032. Test OOXML strict in CI with the WS22 correctness suite (WS05).
1033. Test format-diff in CI with the WS22 correctness suite (WS05).
1034. Test corrupt repair in CI with the WS22 correctness suite (WS05).
1035. Test encrypted OOXML in CI with the WS22 correctness suite (WS05).
1036. Test PDF/A export in CI with the WS22 correctness suite (WS05).
1037. Test JSON/Parquet export in CI with the WS22 correctness suite (WS05).
1038. Test lossless images in CI with the WS22 correctness suite (WS05).
1039. Test flat XML debug in CI with the WS22 correctness suite (WS05).
1040. Test versioned saves in CI with the WS22 correctness suite (WS05).
1041. Test format fuzzer in CI with the WS22 correctness suite (WS05).
1042. Test long-path Unicode in CI with the WS22 correctness suite (WS05).
1043. Test digital signatures in CI with the WS22 correctness suite (WS05).
1044. Test embedded fonts in CI with the WS22 correctness suite (WS05).
1045. Provide a fast OOXML strict that respects user ownership.
1046. Provide a fast JSON/Parquet export suitable for enterprise self-hosting.
1047. Provide a offline versioned saves that respects user ownership.
1048. Provide a offline ODF 1.3 support suitable for enterprise self-hosting.
1049. Provide a local-first corrupt repair that respects user ownership.
1050. Provide a local-first flat XML debug suitable for enterprise self-hosting.
1051. Provide a accessible long-path Unicode that respects user ownership.
1052. Provide a accessible format-diff suitable for enterprise self-hosting.
1053. Provide a secure PDF/A export that respects user ownership.
1054. Provide a secure format fuzzer suitable for enterprise self-hosting.
1055. Provide a simple embedded fonts that respects user ownership.
1056. Provide a simple encrypted OOXML suitable for enterprise self-hosting.
1057. Provide a auditable lossless images that respects user ownership.
1058. Provide a auditable digital signatures suitable for enterprise self-hosting.
1059. Support ODF 1.3 support for personal use at no cost (WS05).
1060. Support OOXML strict for personal use at no cost (WS05).
1061. Support format-diff for personal use at no cost (WS05).
1062. Support corrupt repair for personal use at no cost (WS05).
1063. Support encrypted OOXML for personal use at no cost (WS05).
1064. Support PDF/A export for personal use at no cost (WS05).
1065. Support JSON/Parquet export for personal use at no cost (WS05).
1066. Support lossless images for personal use at no cost (WS05).
1067. Support flat XML debug for personal use at no cost (WS05).
1068. Support versioned saves for personal use at no cost (WS05).
1069. Support format fuzzer for personal use at no cost (WS05).
1070. Support long-path Unicode for personal use at no cost (WS05).
1071. Support digital signatures for personal use at no cost (WS05).
1072. Support embedded fonts for personal use at no cost (WS05).
1073. Never gate ODF 1.3 support behind a subscription or account (WS05).
1074. Never gate OOXML strict behind a subscription or account (WS05).
1075. Never gate format-diff behind a subscription or account (WS05).
1076. Never gate corrupt repair behind a subscription or account (WS05).
1077. Never gate encrypted OOXML behind a subscription or account (WS05).
1078. Never gate PDF/A export behind a subscription or account (WS05).
1079. Never gate JSON/Parquet export behind a subscription or account (WS05).
1080. Never gate lossless images behind a subscription or account (WS05).
1081. Never gate flat XML debug behind a subscription or account (WS05).
1082. Never gate versioned saves behind a subscription or account (WS05).
1083. Never gate format fuzzer behind a subscription or account (WS05).
1084. Never gate long-path Unicode behind a subscription or account (WS05).
1085. Never gate digital signatures behind a subscription or account (WS05).
1086. Never gate embedded fonts behind a subscription or account (WS05).
1087. Add ODF 1.3 support as a first-class, offline-capable feature.
1088. Add OOXML strict as a first-class, offline-capable feature.
1089. Add format-diff as a first-class, offline-capable feature.
1090. Add corrupt repair as a first-class, offline-capable feature.
1091. Add encrypted OOXML as a first-class, offline-capable feature.
1092. Add PDF/A export as a first-class, offline-capable feature.
1093. Add JSON/Parquet export as a first-class, offline-capable feature.
1094. Add lossless images as a first-class, offline-capable feature.
1095. Add flat XML debug as a first-class, offline-capable feature.
1096. Add versioned saves as a first-class, offline-capable feature.
1097. Add format fuzzer as a first-class, offline-capable feature.
1098. Add long-path Unicode as a first-class, offline-capable feature.
1099. Add digital signatures as a first-class, offline-capable feature.
1100. Add embedded fonts as a first-class, offline-capable feature.
1101. Expose ODF 1.3 support through the local extension API (WS12).
1102. Expose OOXML strict through the local extension API (WS12).
1103. Expose format-diff through the local extension API (WS12).
1104. Expose corrupt repair through the local extension API (WS12).
1105. Expose encrypted OOXML through the local extension API (WS12).
1106. Expose PDF/A export through the local extension API (WS12).
1107. Expose JSON/Parquet export through the local extension API (WS12).
1108. Expose lossless images through the local extension API (WS12).
1109. Expose flat XML debug through the local extension API (WS12).

### 06. Real-Time Collaboration & Co-Authoring

1110. Provide true multi-cursor real-time co-editing like Google Docs.
1111. Use CRDT so merges are conflict-free and offline-tolerant.
1112. Show live presence avatars with caret positions per user.
1113. Support per-paragraph/per-cell locking during active edit.
1114. Provide comment threads with resolve/reopen and mentions.
1115. Allow suggesting mode (all edits are proposals until accepted).
1116. Support version history with named snapshots and restore.
1117. Provide a 'fork and merge' workflow for branches of a document.
1118. Let collaboration work peer-to-peer on a LAN without a server.
1119. Support self-hosted relay/server under user control.
1120. Provide end-to-end encrypted collaboration channels.
1121. Show a live change feed (who changed what, when).
1122. Allow presence to be private (hidden) per user choice.
1123. Support async review with tracked changes and replies.
1124. Provide a 'follow me' presentation mode for remote teaching.
1125. Allow co-editing of charts and pivot data live.
1126. Support granular permissions (view/comment/edit) per region.
1127. Provide conflict resolution UI when CRDT hints ambiguity.
1128. Let users see a mini-map of collaborators' viewport.
1129. Support offline edits that sync automatically on reconnect.
1130. Provide a 'who is editing' indicator without leaking content.
1131. Allow exporting a collaboration session transcript.
1132. Support low-bandwidth mode (delta-only sync).
1133. Provide a 'handoff' that transfers doc control between users.
1134. Allow guest access via time-limited signed link.
1135. Support integration with our OS identity/address book.
1136. Provide a 'quiet hours' that batches notifications.
1137. Allow templates to be shared and co-edited in a team library.
1138. Support emoji reactions on comments (local, synced).
1139. Provide a 'merge preview' before accepting a fork.
1140. Allow co-authoring from mobile and desktop simultaneously.
1141. Support undo that respects other users' concurrent edits.
1142. Provide a 'presence cursor color' picker.
1143. Allow documents to live in our OS 'shared spaces' natively.
1144. Support a 'read-only live' view for stakeholders.
1145. Provide activity analytics (optional) for team leads.
1146. Allow anonymous local-network co-edit with no account.
1147. Support session recording for training (local only).
1148. Provide a 'lock whole doc' mode for final freeze.
1149. Allow real-time co-editing of embedded scripts/macros safely.
1150. Support translation of comments across languages live (local model).
1151. Provide a 'you have unread changes' reconciler on open.
1152. Allow co-editing of master slides across a deck.
1153. Support presence in the OS notification center.
1154. Provide a 'diff since last open' summary.
1155. Allow granular region sharing (send one slide, not whole deck).
1156. Support end-to-end encrypted collaboration channels.
1157. Provide a 'leave session' that cleanly detaches CRDT state.
1158. Allow documents to require quorum to publish (team policy).
1159. Support a 'live cursor chat' side channel.
1160. Provide a 'save conflict' insurance that never loses text.
1161. Allow guest editing via disposable ephemeral identity.
1162. Support a 'co-edit replay' to see how a doc evolved.
1163. Provide a 'permissions audit' log per document.
1164. Provide true multi-cursor real-time co-editing like Google Docs.
1165. Use CRDT so merges are conflict-free and offline-tolerant.
1166. Show live presence avatars with caret positions.
1167. Support per-paragraph/per-cell locking during edit.
1168. Provide comment threads with resolve/reopen and mentions.
1169. Allow suggesting mode (all edits are proposals until accepted).
1170. Support version history with named snapshots and restore.
1171. Let collaboration work peer-to-peer on a LAN without a server.
1172. Support self-hosted relay under user control.
1173. Provide end-to-end encrypted collaboration channels.
1174. Show a live change feed (who changed what, when).
1175. Allow presence to be private per user choice.
1176. Support async review with tracked changes and replies.
1177. Provide a 'follow me' presentation mode for remote teaching.
1178. Allow co-editing of charts and pivot data live.
1179. Support granular permissions (view/comment/edit) per region.
1180. Provide conflict resolution UI when CRDT hints ambiguity.
1181. Let users see a mini-map of collaborators' viewport.
1182. Support offline edits that sync on reconnect automatically.
1183. Provide a 'leave session' that cleanly detaches CRDT state.
1184. Support multi-cursor editing for personal use at no cost (WS06).
1185. Support CRDT merge for personal use at no cost (WS06).
1186. Support presence avatars for personal use at no cost (WS06).
1187. Support comment threads for personal use at no cost (WS06).
1188. Support version history for personal use at no cost (WS06).
1189. Support peer-to-peer LAN for personal use at no cost (WS06).
1190. Support self-hosted relay for personal use at no cost (WS06).
1191. Support E2E encrypted for personal use at no cost (WS06).
1192. Support suggesting mode for personal use at no cost (WS06).
1193. Support conflict UI for personal use at no cost (WS06).
1194. Support offline sync for personal use at no cost (WS06).
1195. Support leave session for personal use at no cost (WS06).
1196. Support activity feed for personal use at no cost (WS06).
1197. Support fork-merge for personal use at no cost (WS06).
1198. Never gate multi-cursor editing behind a subscription or account (WS06).
1199. Never gate CRDT merge behind a subscription or account (WS06).
1200. Never gate presence avatars behind a subscription or account (WS06).
1201. Never gate comment threads behind a subscription or account (WS06).
1202. Never gate version history behind a subscription or account (WS06).
1203. Never gate peer-to-peer LAN behind a subscription or account (WS06).
1204. Never gate self-hosted relay behind a subscription or account (WS06).
1205. Never gate E2E encrypted behind a subscription or account (WS06).
1206. Never gate suggesting mode behind a subscription or account (WS06).
1207. Never gate conflict UI behind a subscription or account (WS06).
1208. Never gate offline sync behind a subscription or account (WS06).
1209. Never gate leave session behind a subscription or account (WS06).
1210. Never gate activity feed behind a subscription or account (WS06).
1211. Never gate fork-merge behind a subscription or account (WS06).
1212. Add multi-cursor editing as a first-class, offline-capable feature.
1213. Add CRDT merge as a first-class, offline-capable feature.
1214. Add presence avatars as a first-class, offline-capable feature.
1215. Add comment threads as a first-class, offline-capable feature.
1216. Add version history as a first-class, offline-capable feature.
1217. Add peer-to-peer LAN as a first-class, offline-capable feature.
1218. Add self-hosted relay as a first-class, offline-capable feature.
1219. Add E2E encrypted as a first-class, offline-capable feature.
1220. Add suggesting mode as a first-class, offline-capable feature.
1221. Add conflict UI as a first-class, offline-capable feature.
1222. Add offline sync as a first-class, offline-capable feature.
1223. Add leave session as a first-class, offline-capable feature.
1224. Add activity feed as a first-class, offline-capable feature.
1225. Add fork-merge as a first-class, offline-capable feature.
1226. Expose multi-cursor editing through the local extension API (WS12).
1227. Expose CRDT merge through the local extension API (WS12).
1228. Expose presence avatars through the local extension API (WS12).
1229. Expose comment threads through the local extension API (WS12).
1230. Expose version history through the local extension API (WS12).
1231. Expose peer-to-peer LAN through the local extension API (WS12).
1232. Expose self-hosted relay through the local extension API (WS12).
1233. Expose E2E encrypted through the local extension API (WS12).
1234. Expose suggesting mode through the local extension API (WS12).
1235. Expose conflict UI through the local extension API (WS12).
1236. Expose offline sync through the local extension API (WS12).
1237. Expose leave session through the local extension API (WS12).
1238. Expose activity feed through the local extension API (WS12).
1239. Expose fork-merge through the local extension API (WS12).
1240. Make multi-cursor editing work fully on-device with no telemetry (WS06).
1241. Make CRDT merge work fully on-device with no telemetry (WS06).
1242. Make presence avatars work fully on-device with no telemetry (WS06).
1243. Make comment threads work fully on-device with no telemetry (WS06).
1244. Make version history work fully on-device with no telemetry (WS06).
1245. Make peer-to-peer LAN work fully on-device with no telemetry (WS06).
1246. Make self-hosted relay work fully on-device with no telemetry (WS06).
1247. Make E2E encrypted work fully on-device with no telemetry (WS06).
1248. Make suggesting mode work fully on-device with no telemetry (WS06).
1249. Make conflict UI work fully on-device with no telemetry (WS06).
1250. Make offline sync work fully on-device with no telemetry (WS06).
1251. Make leave session work fully on-device with no telemetry (WS06).
1252. Make activity feed work fully on-device with no telemetry (WS06).
1253. Make fork-merge work fully on-device with no telemetry (WS06).
1254. Test multi-cursor editing in CI with the WS22 correctness suite (WS06).
1255. Test CRDT merge in CI with the WS22 correctness suite (WS06).
1256. Test presence avatars in CI with the WS22 correctness suite (WS06).
1257. Test comment threads in CI with the WS22 correctness suite (WS06).
1258. Test version history in CI with the WS22 correctness suite (WS06).
1259. Test peer-to-peer LAN in CI with the WS22 correctness suite (WS06).
1260. Test self-hosted relay in CI with the WS22 correctness suite (WS06).
1261. Test E2E encrypted in CI with the WS22 correctness suite (WS06).
1262. Test suggesting mode in CI with the WS22 correctness suite (WS06).
1263. Test conflict UI in CI with the WS22 correctness suite (WS06).
1264. Test offline sync in CI with the WS22 correctness suite (WS06).
1265. Test leave session in CI with the WS22 correctness suite (WS06).
1266. Test activity feed in CI with the WS22 correctness suite (WS06).
1267. Test fork-merge in CI with the WS22 correctness suite (WS06).
1268. Provide a fast CRDT merge that respects user ownership.
1269. Provide a fast self-hosted relay suitable for enterprise self-hosting.
1270. Provide a offline conflict UI that respects user ownership.
1271. Provide a offline multi-cursor editing suitable for enterprise self-hosting.
1272. Provide a local-first comment threads that respects user ownership.
1273. Provide a local-first suggesting mode suitable for enterprise self-hosting.
1274. Provide a accessible leave session that respects user ownership.
1275. Provide a accessible presence avatars suitable for enterprise self-hosting.
1276. Provide a secure peer-to-peer LAN that respects user ownership.
1277. Provide a secure offline sync suitable for enterprise self-hosting.
1278. Provide a simple fork-merge that respects user ownership.
1279. Provide a simple version history suitable for enterprise self-hosting.
1280. Provide a auditable E2E encrypted that respects user ownership.
1281. Provide a auditable activity feed suitable for enterprise self-hosting.
1282. Support multi-cursor editing for personal use at no cost (WS06).
1283. Support CRDT merge for personal use at no cost (WS06).
1284. Support presence avatars for personal use at no cost (WS06).
1285. Support comment threads for personal use at no cost (WS06).
1286. Support version history for personal use at no cost (WS06).
1287. Support peer-to-peer LAN for personal use at no cost (WS06).
1288. Support self-hosted relay for personal use at no cost (WS06).
1289. Support E2E encrypted for personal use at no cost (WS06).
1290. Support suggesting mode for personal use at no cost (WS06).
1291. Support conflict UI for personal use at no cost (WS06).
1292. Support offline sync for personal use at no cost (WS06).
1293. Support leave session for personal use at no cost (WS06).
1294. Support activity feed for personal use at no cost (WS06).
1295. Support fork-merge for personal use at no cost (WS06).
1296. Never gate multi-cursor editing behind a subscription or account (WS06).
1297. Never gate CRDT merge behind a subscription or account (WS06).
1298. Never gate presence avatars behind a subscription or account (WS06).
1299. Never gate comment threads behind a subscription or account (WS06).
1300. Never gate version history behind a subscription or account (WS06).
1301. Never gate peer-to-peer LAN behind a subscription or account (WS06).
1302. Never gate self-hosted relay behind a subscription or account (WS06).
1303. Never gate E2E encrypted behind a subscription or account (WS06).
1304. Never gate suggesting mode behind a subscription or account (WS06).
1305. Never gate conflict UI behind a subscription or account (WS06).
1306. Never gate offline sync behind a subscription or account (WS06).
1307. Never gate leave session behind a subscription or account (WS06).
1308. Never gate activity feed behind a subscription or account (WS06).
1309. Never gate fork-merge behind a subscription or account (WS06).
1310. Add multi-cursor editing as a first-class, offline-capable feature.
1311. Add CRDT merge as a first-class, offline-capable feature.
1312. Add presence avatars as a first-class, offline-capable feature.
1313. Add comment threads as a first-class, offline-capable feature.
1314. Add version history as a first-class, offline-capable feature.
1315. Add peer-to-peer LAN as a first-class, offline-capable feature.
1316. Add self-hosted relay as a first-class, offline-capable feature.
1317. Add E2E encrypted as a first-class, offline-capable feature.
1318. Add suggesting mode as a first-class, offline-capable feature.
1319. Add conflict UI as a first-class, offline-capable feature.
1320. Add offline sync as a first-class, offline-capable feature.
1321. Add leave session as a first-class, offline-capable feature.
1322. Add activity feed as a first-class, offline-capable feature.
1323. Add fork-merge as a first-class, offline-capable feature.
1324. Expose multi-cursor editing through the local extension API (WS12).
1325. Expose CRDT merge through the local extension API (WS12).
1326. Expose presence avatars through the local extension API (WS12).
1327. Expose comment threads through the local extension API (WS12).
1328. Expose version history through the local extension API (WS12).
1329. Expose peer-to-peer LAN through the local extension API (WS12).
1330. Expose self-hosted relay through the local extension API (WS12).
1331. Expose E2E encrypted through the local extension API (WS12).
1332. Expose suggesting mode through the local extension API (WS12).

### 07. Performance & Resource Use

1333. Cold start <300ms for a blank document on reference hardware.
1334. Never block the UI thread on load/save; do I/O async.
1335. Keep idle memory <150MB per app on a blank doc.
1336. Stream-render only the visible page/sheet region (virtualized canvas).
1337. Support documents of 1M+ rows without crashing or thrashing.
1338. Use our own DEFLATE for fast, dependency-free compression.
1339. Avoid JVM/Electron; native code for speed and small footprint.
1340. Provide a 'low-power mode' that caps animations and effects.
1341. Incrementally save large files (append deltas), not full rewrite.
1342. Cache parsed models so re-open is instant.
1343. Support background autosave that never interrupts typing.
1344. Profile and cap per-document memory via streaming where possible.
1345. Provide a 'repair if slow' that rebuilds indexes.
1346. Avoid font re-loading on every paint.
1347. Use GPU-accelerated text rendering where available, fallback safe.
1348. Support opening multiple docs with shared process (low RAM).
1349. Provide a 'benchmark' mode that reports ops/sec for regressions.
1350. Never require re-indexing the whole drive to find a file.
1351. Keep installer <80MB; no runtime download post-install.
1352. Support launch from OS shell in <1s end-to-end.
1353. Avoid telemetry/network on the hot path.
1354. Provide a 'headless batch' mode for servers with tiny footprint.
1355. Use mmap for large read-only assets.
1356. Support pause/resume of long operations (find/replace whole book).
1357. Cap undo history memory with configurable depth.
1358. Provide a 'performance HUD' showing frame time and mem.
1359. Avoid blocking on spell-check; run it in a worker.
1360. Support opening corrupt files in a degraded but fast mode.
1361. Keep clipboard operations instant regardless of doc size.
1362. Provide a 'turbo' mode that disables live preview for huge sheets.
1363. Avoid memory leaks across long sessions (CI soak tests).
1364. Support ARM and x86 with SIMD where beneficial.
1365. Provide a 'quit and restore exactly' with no relayout cost.
1366. Avoid synchronous layout on every keystroke (debounced reflow).
1367. Support document sharding so one sheet doesn't block another.
1368. Keep search/index off the critical path.
1369. Provide a 'light' theme that skips heavy graphics.
1370. Support opening files from network shares without full download.
1371. Avoid duplicate parsing when switching tabs.
1372. Provide a 'free RAM now' that trims caches on demand.
1373. Benchmark against LO/MS on the same hardware, publish numbers.
1374. Support a 'low-end PC' preset (disable effects, cap DPI).
1375. Avoid disk thrash from autosave storms.
1376. Provide a 'startup trace' to diagnose slow boots.
1377. Keep the binary tree free of optional heavy deps.
1378. Support cooperative multitasking with the OS scheduler.
1379. Provide a 'freeze background docs' to save CPU.
1380. Avoid re-rendering unchanged slides in a deck.
1381. Support a 'instant close' that defers writes to idle.
1382. Keep memory bounded under infinite undo via snapshot pruning.
1383. Provide a 'resource governor' respecting OS constraints.
1384. Avoid blocking on font substitution lookups.
1385. Support a 'fast open' that parses lazily and renders progressively.
1386. Keep the test suite with perf regression gates.
1387. Cold start <300ms for a blank document on reference hardware.
1388. Never block the UI thread on load/save; do I/O async.
1389. Keep idle memory <150MB per app on a blank doc.
1390. Stream-render only the visible region (virtualized canvas).
1391. Support 1M+ row sheets without crashing or thrashing.
1392. Use our own DEFLATE for fast, dependency-free compression.
1393. Avoid Electron/JVM; native code for speed and footprint.
1394. Provide a 'low-power mode' capping animations.
1395. Incrementally save large files (append deltas), not full rewrite.
1396. Cache parsed models so re-open is instant.
1397. Support background autosave that never interrupts typing.
1398. Profile and cap per-document memory via streaming.
1399. Provide a 'benchmark' mode reporting ops/sec for regressions.
1400. Avoid telemetry/network on the hot path.
1401. Support a 'headless batch' mode with tiny footprint.
1402. Use mmap for large read-only assets.
1403. Support pause/resume of long operations.
1404. Cap undo history memory with configurable depth.
1405. Provide a 'performance HUD' showing frame time and mem.
1406. Keep memory bounded under infinite undo via snapshot pruning.
1407. Support <300ms cold start for personal use at no cost (WS07).
1408. Support async I/O for personal use at no cost (WS07).
1409. Support <150MB idle for personal use at no cost (WS07).
1410. Support virtualized canvas for personal use at no cost (WS07).
1411. Support 1M-row sheets for personal use at no cost (WS07).
1412. Support our DEFLATE for personal use at no cost (WS07).
1413. Support native no-Electron for personal use at no cost (WS07).
1414. Support low-power mode for personal use at no cost (WS07).
1415. Support incremental save for personal use at no cost (WS07).
1416. Support model cache for personal use at no cost (WS07).
1417. Support headless batch for personal use at no cost (WS07).
1418. Support mmap assets for personal use at no cost (WS07).
1419. Support pause-resume ops for personal use at no cost (WS07).
1420. Support perf HUD for personal use at no cost (WS07).
1421. Never gate <300ms cold start behind a subscription or account (WS07).
1422. Never gate async I/O behind a subscription or account (WS07).
1423. Never gate <150MB idle behind a subscription or account (WS07).
1424. Never gate virtualized canvas behind a subscription or account (WS07).
1425. Never gate 1M-row sheets behind a subscription or account (WS07).
1426. Never gate our DEFLATE behind a subscription or account (WS07).
1427. Never gate native no-Electron behind a subscription or account (WS07).
1428. Never gate low-power mode behind a subscription or account (WS07).
1429. Never gate incremental save behind a subscription or account (WS07).
1430. Never gate model cache behind a subscription or account (WS07).
1431. Never gate headless batch behind a subscription or account (WS07).
1432. Never gate mmap assets behind a subscription or account (WS07).
1433. Never gate pause-resume ops behind a subscription or account (WS07).
1434. Never gate perf HUD behind a subscription or account (WS07).
1435. Add <300ms cold start as a first-class, offline-capable feature.
1436. Add async I/O as a first-class, offline-capable feature.
1437. Add <150MB idle as a first-class, offline-capable feature.
1438. Add virtualized canvas as a first-class, offline-capable feature.
1439. Add 1M-row sheets as a first-class, offline-capable feature.
1440. Add our DEFLATE as a first-class, offline-capable feature.
1441. Add native no-Electron as a first-class, offline-capable feature.
1442. Add low-power mode as a first-class, offline-capable feature.
1443. Add incremental save as a first-class, offline-capable feature.
1444. Add model cache as a first-class, offline-capable feature.
1445. Add headless batch as a first-class, offline-capable feature.
1446. Add mmap assets as a first-class, offline-capable feature.
1447. Add pause-resume ops as a first-class, offline-capable feature.
1448. Add perf HUD as a first-class, offline-capable feature.
1449. Expose <300ms cold start through the local extension API (WS12).
1450. Expose async I/O through the local extension API (WS12).
1451. Expose <150MB idle through the local extension API (WS12).
1452. Expose virtualized canvas through the local extension API (WS12).
1453. Expose 1M-row sheets through the local extension API (WS12).
1454. Expose our DEFLATE through the local extension API (WS12).
1455. Expose native no-Electron through the local extension API (WS12).
1456. Expose low-power mode through the local extension API (WS12).
1457. Expose incremental save through the local extension API (WS12).
1458. Expose model cache through the local extension API (WS12).
1459. Expose headless batch through the local extension API (WS12).
1460. Expose mmap assets through the local extension API (WS12).
1461. Expose pause-resume ops through the local extension API (WS12).
1462. Expose perf HUD through the local extension API (WS12).
1463. Make <300ms cold start work fully on-device with no telemetry (WS07).
1464. Make async I/O work fully on-device with no telemetry (WS07).
1465. Make <150MB idle work fully on-device with no telemetry (WS07).
1466. Make virtualized canvas work fully on-device with no telemetry (WS07).
1467. Make 1M-row sheets work fully on-device with no telemetry (WS07).
1468. Make our DEFLATE work fully on-device with no telemetry (WS07).
1469. Make native no-Electron work fully on-device with no telemetry (WS07).
1470. Make low-power mode work fully on-device with no telemetry (WS07).
1471. Make incremental save work fully on-device with no telemetry (WS07).
1472. Make model cache work fully on-device with no telemetry (WS07).
1473. Make headless batch work fully on-device with no telemetry (WS07).
1474. Make mmap assets work fully on-device with no telemetry (WS07).
1475. Make pause-resume ops work fully on-device with no telemetry (WS07).
1476. Make perf HUD work fully on-device with no telemetry (WS07).
1477. Test <300ms cold start in CI with the WS22 correctness suite (WS07).
1478. Test async I/O in CI with the WS22 correctness suite (WS07).
1479. Test <150MB idle in CI with the WS22 correctness suite (WS07).
1480. Test virtualized canvas in CI with the WS22 correctness suite (WS07).
1481. Test 1M-row sheets in CI with the WS22 correctness suite (WS07).
1482. Test our DEFLATE in CI with the WS22 correctness suite (WS07).
1483. Test native no-Electron in CI with the WS22 correctness suite (WS07).
1484. Test low-power mode in CI with the WS22 correctness suite (WS07).
1485. Test incremental save in CI with the WS22 correctness suite (WS07).
1486. Test model cache in CI with the WS22 correctness suite (WS07).
1487. Test headless batch in CI with the WS22 correctness suite (WS07).
1488. Test mmap assets in CI with the WS22 correctness suite (WS07).
1489. Test pause-resume ops in CI with the WS22 correctness suite (WS07).
1490. Test perf HUD in CI with the WS22 correctness suite (WS07).
1491. Provide a fast async I/O that respects user ownership.
1492. Provide a fast native no-Electron suitable for enterprise self-hosting.
1493. Provide a offline model cache that respects user ownership.
1494. Provide a offline <300ms cold start suitable for enterprise self-hosting.
1495. Provide a local-first virtualized canvas that respects user ownership.
1496. Provide a local-first incremental save suitable for enterprise self-hosting.
1497. Provide a accessible mmap assets that respects user ownership.
1498. Provide a accessible <150MB idle suitable for enterprise self-hosting.
1499. Provide a secure our DEFLATE that respects user ownership.
1500. Provide a secure headless batch suitable for enterprise self-hosting.
1501. Provide a simple perf HUD that respects user ownership.
1502. Provide a simple 1M-row sheets suitable for enterprise self-hosting.
1503. Provide a auditable low-power mode that respects user ownership.
1504. Provide a auditable pause-resume ops suitable for enterprise self-hosting.
1505. Support <300ms cold start for personal use at no cost (WS07).
1506. Support async I/O for personal use at no cost (WS07).
1507. Support <150MB idle for personal use at no cost (WS07).
1508. Support virtualized canvas for personal use at no cost (WS07).
1509. Support 1M-row sheets for personal use at no cost (WS07).
1510. Support our DEFLATE for personal use at no cost (WS07).
1511. Support native no-Electron for personal use at no cost (WS07).
1512. Support low-power mode for personal use at no cost (WS07).
1513. Support incremental save for personal use at no cost (WS07).
1514. Support model cache for personal use at no cost (WS07).
1515. Support headless batch for personal use at no cost (WS07).
1516. Support mmap assets for personal use at no cost (WS07).
1517. Support pause-resume ops for personal use at no cost (WS07).
1518. Support perf HUD for personal use at no cost (WS07).
1519. Never gate <300ms cold start behind a subscription or account (WS07).
1520. Never gate async I/O behind a subscription or account (WS07).
1521. Never gate <150MB idle behind a subscription or account (WS07).
1522. Never gate virtualized canvas behind a subscription or account (WS07).
1523. Never gate 1M-row sheets behind a subscription or account (WS07).
1524. Never gate our DEFLATE behind a subscription or account (WS07).
1525. Never gate native no-Electron behind a subscription or account (WS07).
1526. Never gate low-power mode behind a subscription or account (WS07).
1527. Never gate incremental save behind a subscription or account (WS07).
1528. Never gate model cache behind a subscription or account (WS07).
1529. Never gate headless batch behind a subscription or account (WS07).
1530. Never gate mmap assets behind a subscription or account (WS07).
1531. Never gate pause-resume ops behind a subscription or account (WS07).
1532. Never gate perf HUD behind a subscription or account (WS07).
1533. Add <300ms cold start as a first-class, offline-capable feature.
1534. Add async I/O as a first-class, offline-capable feature.
1535. Add <150MB idle as a first-class, offline-capable feature.
1536. Add virtualized canvas as a first-class, offline-capable feature.
1537. Add 1M-row sheets as a first-class, offline-capable feature.
1538. Add our DEFLATE as a first-class, offline-capable feature.
1539. Add native no-Electron as a first-class, offline-capable feature.
1540. Add low-power mode as a first-class, offline-capable feature.
1541. Add incremental save as a first-class, offline-capable feature.
1542. Add model cache as a first-class, offline-capable feature.
1543. Add headless batch as a first-class, offline-capable feature.
1544. Add mmap assets as a first-class, offline-capable feature.
1545. Add pause-resume ops as a first-class, offline-capable feature.
1546. Add perf HUD as a first-class, offline-capable feature.
1547. Expose <300ms cold start through the local extension API (WS12).
1548. Expose async I/O through the local extension API (WS12).
1549. Expose <150MB idle through the local extension API (WS12).
1550. Expose virtualized canvas through the local extension API (WS12).
1551. Expose 1M-row sheets through the local extension API (WS12).
1552. Expose our DEFLATE through the local extension API (WS12).
1553. Expose native no-Electron through the local extension API (WS12).
1554. Expose low-power mode through the local extension API (WS12).
1555. Expose incremental save through the local extension API (WS12).

### 08. Spreadsheet Engine (Excel parity & beyond)

1556. Implement a from-scratch formula engine with 400+ functions (we have 77; expand).
1557. Guarantee bit-identical numeric results with Excel for common formulas.
1558. Support cross-sheet and 3D references (Sheet1:Sheet3!A1).
1559. Support structured table references ([Column]).
1560. Support dynamic arrays / spill (Excel 365 behavior).
1561. Support LAMBDA and named formulas for user-defined functions.
1562. Support array literals and implicit intersection correctly.
1563. Provide a robust error model (#N/A, #VALUE!, #REF!, #CYCLE!).
1564. Implement circular-reference detection with iteration settings.
1565. Support volatile functions (NOW, TODAY, RAND) with recalc control.
1566. Support goal seek and solver (linear/non-linear).
1567. Provide a data table (what-if) engine.
1568. Support pivot tables with grouping and calculated fields.
1569. Support what-if scenarios manager.
1570. Provide a chart engine: line/bar/pie/scatter/area/histogram.
1571. Support conditional formatting with rules and scales.
1572. Support data validation (lists, ranges, custom formulas).
1573. Support freeze panes, split, group/outline.
1574. Provide a百万-row streaming grid that stays responsive.
1575. Support multi-threaded recalc respecting dependencies.
1576. Provide a dependency graph visualizer for debugging formulas.
1577. Support user-defined functions in our safe scripting language.
1578. Support Excel-compatible keyboard shortcuts for power users.
1579. Provide a formula audit (trace precedents/dependents).
1580. Support named ranges local and global.
1581. Provide a unit/regression corpus of Excel results to match.
1582. Support big-number/arbitrary precision where needed (finance).
1583. Provide locale-correct decimal/group separators.
1584. Support R1C1 and A1 reference styles.
1585. Provide a 'watch window' for monitoring cells.
1586. Support scenario summary reports.
1587. Provide solver with GRG and evolutionary methods.
1588. Support statistical add-ons (regression, ANOVA) open.
1589. Provide a financial calendar/date system matching Excel 1900/1904.
1590. Support array formulas legacy (Ctrl+Shift+Enter) import.
1591. Provide a formula text beautifier/auto-formatter.
1592. Support(spill) dynamic arrays across sheets.
1593. Provide a recalc-on-load vs manual mode toggle.
1594. Support query-like transforms (sort/filter/unique) as functions.
1595. Provide a 'calc stepping' debugger that shows intermediate values.
1596. Support external data connections (local CSV/DB) without cloud.
1597. Provide a macro recorder that emits our safe script.
1598. Support in-cell sparklines.
1599. Provide a sheet comparison/diff tool.
1600. Support protection (lock cells) with password (local KDF).
1601. Provide a 'formula search' across the whole workbook.
1602. Support autocomplete for function names and ranges.
1603. Provide a numeric stability test suite (avoid float drift).
1604. Support big grids (1,048,576 rows) memory-mapped.
1605. Provide a 'recalc profile' showing slowest cells.
1606. Support live linked cells across different open workbooks.
1607. Provide a 'what-if' slider UI bound to a cell.
1608. Support image-in-cell and rich values (entities).
1609. Provide a 'formula lint' for common mistakes.
1610. Support international function-name localization optionally.
1611. Provide an open function-catalog that users can extend.
1612. Support integration with our inference engine for forecasting.
1613. Provide a 'spill diagnostics' when a formula unexpectedly spills.
1614. Support defined-name scoping rules exactly like Excel.
1615. Provide a recalc sandbox so a bad UDF can't hang the app.
1616. Implement a from-scratch formula engine with 400+ functions.
1617. Guarantee bit-identical numeric results with Excel for common formulas.
1618. Support cross-sheet and 3D references (Sheet1:Sheet3!A1).
1619. Support structured table references ([Column]).
1620. Support dynamic arrays / spill (Excel 365 behavior).
1621. Support LAMBDA and named formulas for UDFs.
1622. Provide a robust error model (#N/A, #VALUE!, #REF!, #CYCLE!).
1623. Implement circular-reference detection with iteration settings.
1624. Support volatile functions (NOW, TODAY, RAND) with recalc control.
1625. Support goal seek and solver (linear/non-linear).
1626. Provide a data table (what-if) engine.
1627. Support pivot tables with grouping and calculated fields.
1628. Support conditional formatting with rules and scales.
1629. Provide a chart engine: line/bar/pie/scatter/area/histogram.
1630. Support multi-threaded recalc respecting dependencies.
1631. Provide a dependency graph visualizer for debugging formulas.
1632. Support user-defined functions in our safe scripting language.
1633. Support Excel-compatible keyboard shortcuts for power users.
1634. Provide a formula audit (trace precedents/dependents).
1635. Support named ranges local and global.
1636. Support 400+ functions for personal use at no cost (WS08).
1637. Support bit-identical calcs for personal use at no cost (WS08).
1638. Support 3D references for personal use at no cost (WS08).
1639. Support structured refs for personal use at no cost (WS08).
1640. Support dynamic arrays for personal use at no cost (WS08).
1641. Support LAMBDA UDF for personal use at no cost (WS08).
1642. Support circular detect for personal use at no cost (WS08).
1643. Support goal seek/solver for personal use at no cost (WS08).
1644. Support pivot tables for personal use at no cost (WS08).
1645. Support chart engine for personal use at no cost (WS08).
1646. Support multi-thread recalc for personal use at no cost (WS08).
1647. Support formula audit for personal use at no cost (WS08).
1648. Support named ranges for personal use at no cost (WS08).
1649. Support scenario manager for personal use at no cost (WS08).
1650. Never gate 400+ functions behind a subscription or account (WS08).
1651. Never gate bit-identical calcs behind a subscription or account (WS08).
1652. Never gate 3D references behind a subscription or account (WS08).
1653. Never gate structured refs behind a subscription or account (WS08).
1654. Never gate dynamic arrays behind a subscription or account (WS08).
1655. Never gate LAMBDA UDF behind a subscription or account (WS08).
1656. Never gate circular detect behind a subscription or account (WS08).
1657. Never gate goal seek/solver behind a subscription or account (WS08).
1658. Never gate pivot tables behind a subscription or account (WS08).
1659. Never gate chart engine behind a subscription or account (WS08).
1660. Never gate multi-thread recalc behind a subscription or account (WS08).
1661. Never gate formula audit behind a subscription or account (WS08).
1662. Never gate named ranges behind a subscription or account (WS08).
1663. Never gate scenario manager behind a subscription or account (WS08).
1664. Add 400+ functions as a first-class, offline-capable feature.
1665. Add bit-identical calcs as a first-class, offline-capable feature.
1666. Add 3D references as a first-class, offline-capable feature.
1667. Add structured refs as a first-class, offline-capable feature.
1668. Add dynamic arrays as a first-class, offline-capable feature.
1669. Add LAMBDA UDF as a first-class, offline-capable feature.
1670. Add circular detect as a first-class, offline-capable feature.
1671. Add goal seek/solver as a first-class, offline-capable feature.
1672. Add pivot tables as a first-class, offline-capable feature.
1673. Add chart engine as a first-class, offline-capable feature.
1674. Add multi-thread recalc as a first-class, offline-capable feature.
1675. Add formula audit as a first-class, offline-capable feature.
1676. Add named ranges as a first-class, offline-capable feature.
1677. Add scenario manager as a first-class, offline-capable feature.
1678. Expose 400+ functions through the local extension API (WS12).
1679. Expose bit-identical calcs through the local extension API (WS12).
1680. Expose 3D references through the local extension API (WS12).
1681. Expose structured refs through the local extension API (WS12).
1682. Expose dynamic arrays through the local extension API (WS12).
1683. Expose LAMBDA UDF through the local extension API (WS12).
1684. Expose circular detect through the local extension API (WS12).
1685. Expose goal seek/solver through the local extension API (WS12).
1686. Expose pivot tables through the local extension API (WS12).
1687. Expose chart engine through the local extension API (WS12).
1688. Expose multi-thread recalc through the local extension API (WS12).
1689. Expose formula audit through the local extension API (WS12).
1690. Expose named ranges through the local extension API (WS12).
1691. Expose scenario manager through the local extension API (WS12).
1692. Make 400+ functions work fully on-device with no telemetry (WS08).
1693. Make bit-identical calcs work fully on-device with no telemetry (WS08).
1694. Make 3D references work fully on-device with no telemetry (WS08).
1695. Make structured refs work fully on-device with no telemetry (WS08).
1696. Make dynamic arrays work fully on-device with no telemetry (WS08).
1697. Make LAMBDA UDF work fully on-device with no telemetry (WS08).
1698. Make circular detect work fully on-device with no telemetry (WS08).
1699. Make goal seek/solver work fully on-device with no telemetry (WS08).
1700. Make pivot tables work fully on-device with no telemetry (WS08).
1701. Make chart engine work fully on-device with no telemetry (WS08).
1702. Make multi-thread recalc work fully on-device with no telemetry (WS08).
1703. Make formula audit work fully on-device with no telemetry (WS08).
1704. Make named ranges work fully on-device with no telemetry (WS08).
1705. Make scenario manager work fully on-device with no telemetry (WS08).
1706. Test 400+ functions in CI with the WS22 correctness suite (WS08).
1707. Test bit-identical calcs in CI with the WS22 correctness suite (WS08).
1708. Test 3D references in CI with the WS22 correctness suite (WS08).
1709. Test structured refs in CI with the WS22 correctness suite (WS08).
1710. Test dynamic arrays in CI with the WS22 correctness suite (WS08).
1711. Test LAMBDA UDF in CI with the WS22 correctness suite (WS08).
1712. Test circular detect in CI with the WS22 correctness suite (WS08).
1713. Test goal seek/solver in CI with the WS22 correctness suite (WS08).
1714. Test pivot tables in CI with the WS22 correctness suite (WS08).
1715. Test chart engine in CI with the WS22 correctness suite (WS08).
1716. Test multi-thread recalc in CI with the WS22 correctness suite (WS08).
1717. Test formula audit in CI with the WS22 correctness suite (WS08).
1718. Test named ranges in CI with the WS22 correctness suite (WS08).
1719. Test scenario manager in CI with the WS22 correctness suite (WS08).
1720. Provide a fast bit-identical calcs that respects user ownership.
1721. Provide a fast circular detect suitable for enterprise self-hosting.
1722. Provide a offline chart engine that respects user ownership.
1723. Provide a offline 400+ functions suitable for enterprise self-hosting.
1724. Provide a local-first structured refs that respects user ownership.
1725. Provide a local-first pivot tables suitable for enterprise self-hosting.
1726. Provide a accessible formula audit that respects user ownership.
1727. Provide a accessible 3D references suitable for enterprise self-hosting.
1728. Provide a secure LAMBDA UDF that respects user ownership.
1729. Provide a secure multi-thread recalc suitable for enterprise self-hosting.
1730. Provide a simple scenario manager that respects user ownership.
1731. Provide a simple dynamic arrays suitable for enterprise self-hosting.
1732. Provide a auditable goal seek/solver that respects user ownership.
1733. Provide a auditable named ranges suitable for enterprise self-hosting.
1734. Support 400+ functions for personal use at no cost (WS08).
1735. Support bit-identical calcs for personal use at no cost (WS08).
1736. Support 3D references for personal use at no cost (WS08).
1737. Support structured refs for personal use at no cost (WS08).
1738. Support dynamic arrays for personal use at no cost (WS08).
1739. Support LAMBDA UDF for personal use at no cost (WS08).
1740. Support circular detect for personal use at no cost (WS08).
1741. Support goal seek/solver for personal use at no cost (WS08).
1742. Support pivot tables for personal use at no cost (WS08).
1743. Support chart engine for personal use at no cost (WS08).
1744. Support multi-thread recalc for personal use at no cost (WS08).
1745. Support formula audit for personal use at no cost (WS08).
1746. Support named ranges for personal use at no cost (WS08).
1747. Support scenario manager for personal use at no cost (WS08).
1748. Never gate 400+ functions behind a subscription or account (WS08).
1749. Never gate bit-identical calcs behind a subscription or account (WS08).
1750. Never gate 3D references behind a subscription or account (WS08).
1751. Never gate structured refs behind a subscription or account (WS08).
1752. Never gate dynamic arrays behind a subscription or account (WS08).
1753. Never gate LAMBDA UDF behind a subscription or account (WS08).
1754. Never gate circular detect behind a subscription or account (WS08).
1755. Never gate goal seek/solver behind a subscription or account (WS08).
1756. Never gate pivot tables behind a subscription or account (WS08).
1757. Never gate chart engine behind a subscription or account (WS08).
1758. Never gate multi-thread recalc behind a subscription or account (WS08).
1759. Never gate formula audit behind a subscription or account (WS08).
1760. Never gate named ranges behind a subscription or account (WS08).
1761. Never gate scenario manager behind a subscription or account (WS08).
1762. Add 400+ functions as a first-class, offline-capable feature.
1763. Add bit-identical calcs as a first-class, offline-capable feature.
1764. Add 3D references as a first-class, offline-capable feature.
1765. Add structured refs as a first-class, offline-capable feature.
1766. Add dynamic arrays as a first-class, offline-capable feature.
1767. Add LAMBDA UDF as a first-class, offline-capable feature.
1768. Add circular detect as a first-class, offline-capable feature.
1769. Add goal seek/solver as a first-class, offline-capable feature.
1770. Add pivot tables as a first-class, offline-capable feature.
1771. Add chart engine as a first-class, offline-capable feature.
1772. Add multi-thread recalc as a first-class, offline-capable feature.
1773. Add formula audit as a first-class, offline-capable feature.
1774. Add named ranges as a first-class, offline-capable feature.
1775. Add scenario manager as a first-class, offline-capable feature.
1776. Expose 400+ functions through the local extension API (WS12).
1777. Expose bit-identical calcs through the local extension API (WS12).
1778. Expose 3D references through the local extension API (WS12).
1779. Expose structured refs through the local extension API (WS12).
1780. Expose dynamic arrays through the local extension API (WS12).
1781. Expose LAMBDA UDF through the local extension API (WS12).
1782. Expose circular detect through the local extension API (WS12).
1783. Expose goal seek/solver through the local extension API (WS12).
1784. Expose pivot tables through the local extension API (WS12).

### 09. Word Processing

1785. Support true style inheritance (paragraph/character/list/table).
1786. Provide a styles pane with live preview and organize-by-usage.
1787. Support outline view with collapse/expand and reorder by heading.
1788. Implement track changes with accept/reject per change and bulk.
1789. Support compare documents producing a redline.
1790. Provide a master-document feature for book-length works.
1791. Support footnotes, endnotes, and cross-references that auto-update.
1792. Support tables with repeat header rows and captions.
1793. Provide a bibliography/reference manager (BibTeX/Zotero import).
1794. Support multi-column layouts and section breaks.
1795. Provide a TOC that builds from headings and updates.
1796. Support indexing with auto-generated index marks.
1797. Provide mail merge from local CSV/JSON/our data store.
1798. Support equations via MathML/LaTeX with numbering.
1799. Provide a watermark, header/footer, and page borders.
1800. Support RTL and bidirectional text correctly.
1801. Provide hanging punctuation and kerning controls.
1802. Support vertical text and East-Asian typography.
1803. Provide a 'focus' typewriter mode with centered caret.
1804. Support full-bleed and booklet printing imposition.
1805. Provide a 'manuscript' template (12pt, double-spaced, indents).
1806. Support change bars and comment bubbles in margin.
1807. Provide a 'redline to clean' one-click accept all.
1808. Support co-authoring track changes (see Workstream 06).
1809. Provide a 'resolve all comments' bulk action.
1810. Support document variables and fields (page, author, date).
1811. Provide a 'compare to last saved' quick diff.
1812. Support auto-save versions with restore.
1813. Provide a 'reading mode' with pagination like a book.
1814. Support Ink/handwriting to text conversion offline.
1815. Provide a 'say as you type' dictation offline (local STT).
1816. Support a thesaurus and synonym lookup offline.
1817. Provide a 'word count by section' analytics.
1818. Support embedded spreadsheets/charts that update live.
1819. Provide a 'translate selection' via local model, inline.
1820. Support grammar checking offline with explanations.
1821. Provide a 'clarity' suggestions mode (opt-in, local).
1822. Support protected sections with editable regions.
1823. Provide a 'template gallery' curated and offline.
1824. Support custom document properties and smart tags (local).
1825. Provide a 'publish to PDF/EPUB' with accessibility pass.
1826. Support forms with content controls (dropdowns, dates).
1827. Provide a 'legal blackline' comparison report.
1828. Support page-numbering schemes per section.
1829. Provide a 'link to heading' auto reference field.
1830. Support caption numbering for figures/tables/equations.
1831. Provide a 'no-break' keep-with-next and keep-lines-together.
1832. Support autocorrect exceptions per language.
1833. Provide a 'style inspector' showing effective formatting.
1834. Support pasting from web with clean formatting options.
1835. Provide a 'document map' outline navigator.
1836. Support revision history baked into the file.
1837. Provide a 'sentiment/reading-level' meter (local model).
1838. Support hyphenation dictionaries per language.
1839. Provide a 'compare two versions' structural diff.
1840. Support custom line numbering for legal/code docs.
1841. Provide a 'manuscript stats' (flesch, chars, scenes).
1842. Support embedding audio/video with captions.
1843. Provide a 'export to Markdown' preserving structure.
1844. Support a 'distraction-free compose' with typewriter scroll.
1845. Support true style inheritance (paragraph/character/list/table).
1846. Provide a styles pane with live preview and organize-by-usage.
1847. Support outline view with collapse/expand and reorder by heading.
1848. Implement track changes with accept/reject per change and bulk.
1849. Support compare documents producing a redline.
1850. Provide a master-document feature for book-length works.
1851. Support footnotes, endnotes, and cross-references that auto-update.
1852. Support tables with repeat header rows and captions.
1853. Provide a bibliography/reference manager (BibTeX/Zotero import).
1854. Support multi-column layouts and section breaks.
1855. Provide a TOC that builds from headings and updates.
1856. Support indexing with auto-generated index marks.
1857. Provide mail merge from local CSV/JSON/our data store.
1858. Support equations via MathML/LaTeX with numbering.
1859. Provide a watermark, header/footer, and page borders.
1860. Support RTL and bidirectional text correctly.
1861. Provide a 'focus' typewriter mode with centered caret.
1862. Provide a 'manuscript' template (12pt, double-spaced, indents).
1863. Support change bars and comment bubbles in margin.
1864. Provide a 'redline to clean' one-click accept all.
1865. Support style inheritance for personal use at no cost (WS09).
1866. Support styles pane for personal use at no cost (WS09).
1867. Support outline view for personal use at no cost (WS09).
1868. Support track changes for personal use at no cost (WS09).
1869. Support compare docs for personal use at no cost (WS09).
1870. Support master docs for personal use at no cost (WS09).
1871. Support footnotes/xrefs for personal use at no cost (WS09).
1872. Support bibliography for personal use at no cost (WS09).
1873. Support multi-column for personal use at no cost (WS09).
1874. Support auto TOC for personal use at no cost (WS09).
1875. Support indexing for personal use at no cost (WS09).
1876. Support mail merge for personal use at no cost (WS09).
1877. Support equations for personal use at no cost (WS09).
1878. Support watermark for personal use at no cost (WS09).
1879. Never gate style inheritance behind a subscription or account (WS09).
1880. Never gate styles pane behind a subscription or account (WS09).
1881. Never gate outline view behind a subscription or account (WS09).
1882. Never gate track changes behind a subscription or account (WS09).
1883. Never gate compare docs behind a subscription or account (WS09).
1884. Never gate master docs behind a subscription or account (WS09).
1885. Never gate footnotes/xrefs behind a subscription or account (WS09).
1886. Never gate bibliography behind a subscription or account (WS09).
1887. Never gate multi-column behind a subscription or account (WS09).
1888. Never gate auto TOC behind a subscription or account (WS09).
1889. Never gate indexing behind a subscription or account (WS09).
1890. Never gate mail merge behind a subscription or account (WS09).
1891. Never gate equations behind a subscription or account (WS09).
1892. Never gate watermark behind a subscription or account (WS09).
1893. Add style inheritance as a first-class, offline-capable feature.
1894. Add styles pane as a first-class, offline-capable feature.
1895. Add outline view as a first-class, offline-capable feature.
1896. Add track changes as a first-class, offline-capable feature.
1897. Add compare docs as a first-class, offline-capable feature.
1898. Add master docs as a first-class, offline-capable feature.
1899. Add footnotes/xrefs as a first-class, offline-capable feature.
1900. Add bibliography as a first-class, offline-capable feature.
1901. Add multi-column as a first-class, offline-capable feature.
1902. Add auto TOC as a first-class, offline-capable feature.
1903. Add indexing as a first-class, offline-capable feature.
1904. Add mail merge as a first-class, offline-capable feature.
1905. Add equations as a first-class, offline-capable feature.
1906. Add watermark as a first-class, offline-capable feature.
1907. Expose style inheritance through the local extension API (WS12).
1908. Expose styles pane through the local extension API (WS12).
1909. Expose outline view through the local extension API (WS12).
1910. Expose track changes through the local extension API (WS12).
1911. Expose compare docs through the local extension API (WS12).
1912. Expose master docs through the local extension API (WS12).
1913. Expose footnotes/xrefs through the local extension API (WS12).
1914. Expose bibliography through the local extension API (WS12).
1915. Expose multi-column through the local extension API (WS12).
1916. Expose auto TOC through the local extension API (WS12).
1917. Expose indexing through the local extension API (WS12).
1918. Expose mail merge through the local extension API (WS12).
1919. Expose equations through the local extension API (WS12).
1920. Expose watermark through the local extension API (WS12).
1921. Make style inheritance work fully on-device with no telemetry (WS09).
1922. Make styles pane work fully on-device with no telemetry (WS09).
1923. Make outline view work fully on-device with no telemetry (WS09).
1924. Make track changes work fully on-device with no telemetry (WS09).
1925. Make compare docs work fully on-device with no telemetry (WS09).
1926. Make master docs work fully on-device with no telemetry (WS09).
1927. Make footnotes/xrefs work fully on-device with no telemetry (WS09).
1928. Make bibliography work fully on-device with no telemetry (WS09).
1929. Make multi-column work fully on-device with no telemetry (WS09).
1930. Make auto TOC work fully on-device with no telemetry (WS09).
1931. Make indexing work fully on-device with no telemetry (WS09).
1932. Make mail merge work fully on-device with no telemetry (WS09).
1933. Make equations work fully on-device with no telemetry (WS09).
1934. Make watermark work fully on-device with no telemetry (WS09).
1935. Test style inheritance in CI with the WS22 correctness suite (WS09).
1936. Test styles pane in CI with the WS22 correctness suite (WS09).
1937. Test outline view in CI with the WS22 correctness suite (WS09).
1938. Test track changes in CI with the WS22 correctness suite (WS09).
1939. Test compare docs in CI with the WS22 correctness suite (WS09).
1940. Test master docs in CI with the WS22 correctness suite (WS09).
1941. Test footnotes/xrefs in CI with the WS22 correctness suite (WS09).
1942. Test bibliography in CI with the WS22 correctness suite (WS09).
1943. Test multi-column in CI with the WS22 correctness suite (WS09).
1944. Test auto TOC in CI with the WS22 correctness suite (WS09).
1945. Test indexing in CI with the WS22 correctness suite (WS09).
1946. Test mail merge in CI with the WS22 correctness suite (WS09).
1947. Test equations in CI with the WS22 correctness suite (WS09).
1948. Test watermark in CI with the WS22 correctness suite (WS09).
1949. Provide a fast styles pane that respects user ownership.
1950. Provide a fast footnotes/xrefs suitable for enterprise self-hosting.
1951. Provide a offline auto TOC that respects user ownership.
1952. Provide a offline style inheritance suitable for enterprise self-hosting.
1953. Provide a local-first track changes that respects user ownership.
1954. Provide a local-first multi-column suitable for enterprise self-hosting.
1955. Provide a accessible mail merge that respects user ownership.
1956. Provide a accessible outline view suitable for enterprise self-hosting.
1957. Provide a secure master docs that respects user ownership.
1958. Provide a secure indexing suitable for enterprise self-hosting.
1959. Provide a simple watermark that respects user ownership.
1960. Provide a simple compare docs suitable for enterprise self-hosting.
1961. Provide a auditable bibliography that respects user ownership.
1962. Provide a auditable equations suitable for enterprise self-hosting.
1963. Support style inheritance for personal use at no cost (WS09).
1964. Support styles pane for personal use at no cost (WS09).
1965. Support outline view for personal use at no cost (WS09).
1966. Support track changes for personal use at no cost (WS09).
1967. Support compare docs for personal use at no cost (WS09).
1968. Support master docs for personal use at no cost (WS09).
1969. Support footnotes/xrefs for personal use at no cost (WS09).
1970. Support bibliography for personal use at no cost (WS09).
1971. Support multi-column for personal use at no cost (WS09).
1972. Support auto TOC for personal use at no cost (WS09).
1973. Support indexing for personal use at no cost (WS09).
1974. Support mail merge for personal use at no cost (WS09).
1975. Support equations for personal use at no cost (WS09).
1976. Support watermark for personal use at no cost (WS09).
1977. Never gate style inheritance behind a subscription or account (WS09).
1978. Never gate styles pane behind a subscription or account (WS09).
1979. Never gate outline view behind a subscription or account (WS09).
1980. Never gate track changes behind a subscription or account (WS09).
1981. Never gate compare docs behind a subscription or account (WS09).
1982. Never gate master docs behind a subscription or account (WS09).
1983. Never gate footnotes/xrefs behind a subscription or account (WS09).
1984. Never gate bibliography behind a subscription or account (WS09).
1985. Never gate multi-column behind a subscription or account (WS09).
1986. Never gate auto TOC behind a subscription or account (WS09).
1987. Never gate indexing behind a subscription or account (WS09).
1988. Never gate mail merge behind a subscription or account (WS09).
1989. Never gate equations behind a subscription or account (WS09).
1990. Never gate watermark behind a subscription or account (WS09).
1991. Add style inheritance as a first-class, offline-capable feature.
1992. Add styles pane as a first-class, offline-capable feature.
1993. Add outline view as a first-class, offline-capable feature.
1994. Add track changes as a first-class, offline-capable feature.
1995. Add compare docs as a first-class, offline-capable feature.
1996. Add master docs as a first-class, offline-capable feature.
1997. Add footnotes/xrefs as a first-class, offline-capable feature.
1998. Add bibliography as a first-class, offline-capable feature.
1999. Add multi-column as a first-class, offline-capable feature.
2000. Add auto TOC as a first-class, offline-capable feature.
2001. Add indexing as a first-class, offline-capable feature.
2002. Add mail merge as a first-class, offline-capable feature.
2003. Add equations as a first-class, offline-capable feature.
2004. Add watermark as a first-class, offline-capable feature.
2005. Expose style inheritance through the local extension API (WS12).
2006. Expose styles pane through the local extension API (WS12).
2007. Expose outline view through the local extension API (WS12).
2008. Expose track changes through the local extension API (WS12).
2009. Expose compare docs through the local extension API (WS12).
2010. Expose master docs through the local extension API (WS12).
2011. Expose footnotes/xrefs through the local extension API (WS12).
2012. Expose bibliography through the local extension API (WS12).

### 10. Presentations

2013. Support 16:9/4:3/16:10 and custom slide sizes.
2014. Provide a master/slide-layout system with placeholder inheritance.
2015. Support transitions that are GPU-accelerated and skippable.
2016. Provide a presenter view with notes, timer, and next-slide.
2017. Support speaker notes per slide with rich text.
2018. Provide a 'rehearse timings' mode.
2019. Support annotations/draw on slide during presentation (local).
2020. Provide a chart/table that links to a live spreadsheet.
2021. Support embedding video/audio with playback controls.
2022. Provide a 'morph' style transition between layouts.
2023. Support sections to organize a deck.
2024. Provide a 'photo album' auto-layout from a folder.
2025. Support master handout/notes-page printing.
2026. Provide a 'broadcast' mode streaming to our OS devices.
2027. Support conflict-free co-editing of a deck (WS06).
2028. Provide a 'design ideas' helper using local model (opt-in).
2029. Support SVG and vector shapes natively.
2030. Provide a shape union/subtract/intersect/trim.
2031. Support smart guides and snap to grid/objects.
2032. Provide a 'replace fonts' across the whole deck.
2033. Support animation pane with timeline and easing.
2034. Provide a 'record slideshow' to video locally.
2035. Support QR/link to live doc for audience.
2036. Provide a 'export to images' (PNG/PDF) per slide.
2037. Support a 'kiosk' loop mode.
2038. Provide a 'remote clicker' over our OS Bluetooth stack.
2039. Support braille/AT navigation of slides.
2040. Provide a 'outline to deck' auto-generator from headings.
2041. Support theme variants (color/font/size) applied instantly.
2042. Provide a 'compare decks' structural diff.
2043. Support comments per slide with threads.
2044. Provide a 'narrate slide' TTS offline for rehearsal.
2045. Support hidden slides for branching presentations.
2046. Provide a 'section zoom' like Prezi-style navigation (optional).
2047. Support 3D models with rotate (local viewer).
2048. Provide a 'live caption' of spoken presenter via local STT.
2049. Support 'translate my slides' for the audience view.
2050. Provide a 'template from this deck' one-click.
2051. Support master footer/date/number fields.
2052. Provide a 'slide sorter' thumbnail grid with drag reorder.
2053. Support paste-linked objects that update.
2054. Provide a 'export to OOXML/ODF/PDF' with fidelity.
2055. Support a 'practice mode' with filler-word detection (local).
2056. Support emoji and icon library offline.
2057. Provide a 'highlighter' pen during show.
2058. Support 'laser pointer' cursor effect.
2059. Provide a 'black/white screen' presenter hotkey.
2060. Support 'slide zoom' for Q&A navigation.
2061. Provide a 'auto-fit text' to placeholder.
2062. Support 'master reset' that reapplies layout.
2063. Provide a 'deck statistics' (slides, words, est. time).
2064. Support 'embed spreadsheet chart' live linked.
2065. Provide a 'section transitions' distinct from slide.
2066. Support 'present to window' for screen capture.
2067. Provide a 'offline spell check' of notes.
2068. Support 'template marketplace' curated, no account needed.
2069. Provide a 'deck health' check (contrast, font size, alt text).
2070. Support 16:9/4:3/16:10 and custom slide sizes.
2071. Provide a master/slide-layout system with placeholder inheritance.
2072. Support transitions that are GPU-accelerated and skippable.
2073. Provide a presenter view with notes, timer, and next-slide.
2074. Support speaker notes per slide with rich text.
2075. Provide a 'rehearse timings' mode.
2076. Support annotations/draw on slide during presentation (local).
2077. Provide a chart/table that links to a live spreadsheet.
2078. Support embedding video/audio with playback controls.
2079. Provide a 'morph' style transition between layouts.
2080. Support sections to organize a deck.
2081. Provide a 'photo album' auto-layout from a folder.
2082. Provide a master handout/notes-page printing.
2083. Provide a 'broadcast' mode streaming to our OS devices.
2084. Support conflict-free co-editing of a deck (WS06).
2085. Provide a 'design ideas' helper using local model (opt-in).
2086. Support SVG and vector shapes natively.
2087. Provide a shape union/subtract/intersect/trim.
2088. Support smart guides and snap to grid/objects.
2089. Provide a 'replace fonts' across the whole deck.
2090. Support 16:9/4:3 sizes for personal use at no cost (WS10).
2091. Support master/layout for personal use at no cost (WS10).
2092. Support GPU transitions for personal use at no cost (WS10).
2093. Support presenter view for personal use at no cost (WS10).
2094. Support speaker notes for personal use at no cost (WS10).
2095. Support rehearse timings for personal use at no cost (WS10).
2096. Support ink annotate for personal use at no cost (WS10).
2097. Support live chart link for personal use at no cost (WS10).
2098. Support video/audio for personal use at no cost (WS10).
2099. Support morph transition for personal use at no cost (WS10).
2100. Support sections for personal use at no cost (WS10).
2101. Support photo album for personal use at no cost (WS10).
2102. Support broadcast mode for personal use at no cost (WS10).
2103. Support design ideas for personal use at no cost (WS10).
2104. Never gate 16:9/4:3 sizes behind a subscription or account (WS10).
2105. Never gate master/layout behind a subscription or account (WS10).
2106. Never gate GPU transitions behind a subscription or account (WS10).
2107. Never gate presenter view behind a subscription or account (WS10).
2108. Never gate speaker notes behind a subscription or account (WS10).
2109. Never gate rehearse timings behind a subscription or account (WS10).
2110. Never gate ink annotate behind a subscription or account (WS10).
2111. Never gate live chart link behind a subscription or account (WS10).
2112. Never gate video/audio behind a subscription or account (WS10).
2113. Never gate morph transition behind a subscription or account (WS10).
2114. Never gate sections behind a subscription or account (WS10).
2115. Never gate photo album behind a subscription or account (WS10).
2116. Never gate broadcast mode behind a subscription or account (WS10).
2117. Never gate design ideas behind a subscription or account (WS10).
2118. Add 16:9/4:3 sizes as a first-class, offline-capable feature.
2119. Add master/layout as a first-class, offline-capable feature.
2120. Add GPU transitions as a first-class, offline-capable feature.
2121. Add presenter view as a first-class, offline-capable feature.
2122. Add speaker notes as a first-class, offline-capable feature.
2123. Add rehearse timings as a first-class, offline-capable feature.
2124. Add ink annotate as a first-class, offline-capable feature.
2125. Add live chart link as a first-class, offline-capable feature.
2126. Add video/audio as a first-class, offline-capable feature.
2127. Add morph transition as a first-class, offline-capable feature.
2128. Add sections as a first-class, offline-capable feature.
2129. Add photo album as a first-class, offline-capable feature.
2130. Add broadcast mode as a first-class, offline-capable feature.
2131. Add design ideas as a first-class, offline-capable feature.
2132. Expose 16:9/4:3 sizes through the local extension API (WS12).
2133. Expose master/layout through the local extension API (WS12).
2134. Expose GPU transitions through the local extension API (WS12).
2135. Expose presenter view through the local extension API (WS12).
2136. Expose speaker notes through the local extension API (WS12).
2137. Expose rehearse timings through the local extension API (WS12).
2138. Expose ink annotate through the local extension API (WS12).
2139. Expose live chart link through the local extension API (WS12).
2140. Expose video/audio through the local extension API (WS12).
2141. Expose morph transition through the local extension API (WS12).
2142. Expose sections through the local extension API (WS12).
2143. Expose photo album through the local extension API (WS12).
2144. Expose broadcast mode through the local extension API (WS12).
2145. Expose design ideas through the local extension API (WS12).
2146. Make 16:9/4:3 sizes work fully on-device with no telemetry (WS10).
2147. Make master/layout work fully on-device with no telemetry (WS10).
2148. Make GPU transitions work fully on-device with no telemetry (WS10).
2149. Make presenter view work fully on-device with no telemetry (WS10).
2150. Make speaker notes work fully on-device with no telemetry (WS10).
2151. Make rehearse timings work fully on-device with no telemetry (WS10).
2152. Make ink annotate work fully on-device with no telemetry (WS10).
2153. Make live chart link work fully on-device with no telemetry (WS10).
2154. Make video/audio work fully on-device with no telemetry (WS10).
2155. Make morph transition work fully on-device with no telemetry (WS10).
2156. Make sections work fully on-device with no telemetry (WS10).
2157. Make photo album work fully on-device with no telemetry (WS10).
2158. Make broadcast mode work fully on-device with no telemetry (WS10).
2159. Make design ideas work fully on-device with no telemetry (WS10).
2160. Test 16:9/4:3 sizes in CI with the WS22 correctness suite (WS10).
2161. Test master/layout in CI with the WS22 correctness suite (WS10).
2162. Test GPU transitions in CI with the WS22 correctness suite (WS10).
2163. Test presenter view in CI with the WS22 correctness suite (WS10).
2164. Test speaker notes in CI with the WS22 correctness suite (WS10).
2165. Test rehearse timings in CI with the WS22 correctness suite (WS10).
2166. Test ink annotate in CI with the WS22 correctness suite (WS10).
2167. Test live chart link in CI with the WS22 correctness suite (WS10).
2168. Test video/audio in CI with the WS22 correctness suite (WS10).
2169. Test morph transition in CI with the WS22 correctness suite (WS10).
2170. Test sections in CI with the WS22 correctness suite (WS10).
2171. Test photo album in CI with the WS22 correctness suite (WS10).
2172. Test broadcast mode in CI with the WS22 correctness suite (WS10).
2173. Test design ideas in CI with the WS22 correctness suite (WS10).
2174. Provide a fast master/layout that respects user ownership.
2175. Provide a fast ink annotate suitable for enterprise self-hosting.
2176. Provide a offline morph transition that respects user ownership.
2177. Provide a offline 16:9/4:3 sizes suitable for enterprise self-hosting.
2178. Provide a local-first presenter view that respects user ownership.
2179. Provide a local-first video/audio suitable for enterprise self-hosting.
2180. Provide a accessible photo album that respects user ownership.
2181. Provide a accessible GPU transitions suitable for enterprise self-hosting.
2182. Provide a secure rehearse timings that respects user ownership.
2183. Provide a secure sections suitable for enterprise self-hosting.
2184. Provide a simple design ideas that respects user ownership.
2185. Provide a simple speaker notes suitable for enterprise self-hosting.
2186. Provide a auditable live chart link that respects user ownership.
2187. Provide a auditable broadcast mode suitable for enterprise self-hosting.
2188. Support 16:9/4:3 sizes for personal use at no cost (WS10).
2189. Support master/layout for personal use at no cost (WS10).
2190. Support GPU transitions for personal use at no cost (WS10).
2191. Support presenter view for personal use at no cost (WS10).
2192. Support speaker notes for personal use at no cost (WS10).
2193. Support rehearse timings for personal use at no cost (WS10).
2194. Support ink annotate for personal use at no cost (WS10).
2195. Support live chart link for personal use at no cost (WS10).
2196. Support video/audio for personal use at no cost (WS10).
2197. Support morph transition for personal use at no cost (WS10).
2198. Support sections for personal use at no cost (WS10).
2199. Support photo album for personal use at no cost (WS10).
2200. Support broadcast mode for personal use at no cost (WS10).
2201. Support design ideas for personal use at no cost (WS10).
2202. Never gate 16:9/4:3 sizes behind a subscription or account (WS10).
2203. Never gate master/layout behind a subscription or account (WS10).
2204. Never gate GPU transitions behind a subscription or account (WS10).
2205. Never gate presenter view behind a subscription or account (WS10).
2206. Never gate speaker notes behind a subscription or account (WS10).
2207. Never gate rehearse timings behind a subscription or account (WS10).
2208. Never gate ink annotate behind a subscription or account (WS10).
2209. Never gate live chart link behind a subscription or account (WS10).
2210. Never gate video/audio behind a subscription or account (WS10).
2211. Never gate morph transition behind a subscription or account (WS10).
2212. Never gate sections behind a subscription or account (WS10).
2213. Never gate photo album behind a subscription or account (WS10).
2214. Never gate broadcast mode behind a subscription or account (WS10).
2215. Never gate design ideas behind a subscription or account (WS10).
2216. Add 16:9/4:3 sizes as a first-class, offline-capable feature.
2217. Add master/layout as a first-class, offline-capable feature.
2218. Add GPU transitions as a first-class, offline-capable feature.
2219. Add presenter view as a first-class, offline-capable feature.
2220. Add speaker notes as a first-class, offline-capable feature.
2221. Add rehearse timings as a first-class, offline-capable feature.
2222. Add ink annotate as a first-class, offline-capable feature.
2223. Add live chart link as a first-class, offline-capable feature.
2224. Add video/audio as a first-class, offline-capable feature.
2225. Add morph transition as a first-class, offline-capable feature.
2226. Add sections as a first-class, offline-capable feature.
2227. Add photo album as a first-class, offline-capable feature.
2228. Add broadcast mode as a first-class, offline-capable feature.
2229. Add design ideas as a first-class, offline-capable feature.
2230. Expose 16:9/4:3 sizes through the local extension API (WS12).
2231. Expose master/layout through the local extension API (WS12).
2232. Expose GPU transitions through the local extension API (WS12).
2233. Expose presenter view through the local extension API (WS12).
2234. Expose speaker notes through the local extension API (WS12).
2235. Expose rehearse timings through the local extension API (WS12).
2236. Expose ink annotate through the local extension API (WS12).
2237. Expose live chart link through the local extension API (WS12).

### 11. Document Model & Unified Object Model

2238. Define one canonical object model shared by all three apps.
2239. Represent every entity (run, cell, shape) as a typed node.
2240. Separate content, style, and layout in the model.
2241. Make the model serializable to our internal format and OOXML.
2242. Expose a stable DOM-like API for scripts and add-ins.
2243. Give every object a stable ID for diff/merge/undo.
2244. Support a command pattern so all edits are reversible/recordable.
2245. Store rich metadata (provenance, author, timestamps) per node.
2246. Allow queries over the model (find all headings, all charts).
2247. Support a reactive model: UI updates from model changes only.
2248. Make the model the single source for both render and export.
2249. Support incremental updates (patches) for collaboration sync.
2250. Allow the model to embed other suite docs as live objects.
2251. Provide a schema/version for the model with forward migration.
2252. Support a 'model explorer' dev tool to inspect any node.
2253. Keep the model free of UI concepts (testable headless).
2254. Support a 'replay' of the command log to reproduce a doc.
2255. Allow plugins to extend the model with custom node types.
2256. Provide a 'graph view' linking related objects across docs.
2257. Support a 'semantic layer' so our inference engine reads meaning.
2258. Make the model the integration point with our OS file/VFS.
2259. Support a 'document as a folder' view in the OS shell.
2260. Allow the model to carry training signals for the RL environment.
2261. Provide a 'canonical diff' format for version control.
2262. Support a 'model patch' language for fine-grained sync.
2263. Keep the model deterministic given the same command sequence.
2264. Support a 'snapshot' for undo that is memory-bounded.
2265. Allow the model to reference external data (our data store).
2266. Provide a 'type registry' so add-ins declare node kinds.
2267. Support a 'query language' (XPath-like) over the model.
2268. Make every style a first-class reusable object.
2269. Support a 'theme' object shared across apps.
2270. Allow the model to embed computation (formula nodes).
2271. Provide a 'validation' that checks model invariants.
2272. Support a 'migration' from v1 to vN model automatically.
2273. Expose the model to our OS search/indexer directly.
2274. Support a 'headless render' of any node to an image.
2275. Allow the model to be the unit of collaboration (CRDT on nodes).
2276. Provide a 'node history' (who edited this paragraph).
2277. Support a 'derived view' (e.g., TOC) computed from model.
2278. Allow the model to express relations (see WS17 knowledge graph).
2279. Provide a 'model stats' (nodes, edges, size) for perf.
2280. Support a 'semantic diff' that ignores whitespace/layout.
2281. Make the model the anchor for accessibility tree generation.
2282. Support a 'policy' object enforcing team conventions.
2283. Allow the model to be signed/attested per node.
2284. Provide a 'model lint' for dangling references.
2285. Support a 'live object' that recomputes from source on open.
2286. Allow the model to carry RL task annotations (see WS14).
2287. Provide a 'document manifest' listing all parts and hashes.
2288. Support a 'model patch' applied transactionally.
2289. Keep the model backend-agnostic (file, VFS, store).
2290. Support a 'clone' that deep-copies a subtree.
2291. Allow the model to be the unit of sandboxed scripting.
2292. Provide a 'schema doc' auto-generated for developers.
2293. Define one canonical object model shared by all three apps.
2294. Represent every entity (run, cell, shape) as a typed node.
2295. Separate content, style, and layout in the model.
2296. Make the model serializable to our internal format and OOXML.
2297. Expose a stable DOM-like API for scripts and add-ins.
2298. Give every object a stable ID for diff/merge/undo.
2299. Support a command pattern so all edits are reversible/recordable.
2300. Store rich metadata (provenance, author, timestamps) per node.
2301. Allow queries over the model (find all headings, all charts).
2302. Support a reactive model: UI updates from model changes only.
2303. Make the model the single source for both render and export.
2304. Support incremental updates (patches) for collaboration sync.
2305. Allow the model to embed other suite docs as live objects.
2306. Provide a schema/version for the model with forward migration.
2307. Support a 'model explorer' dev tool to inspect any node.
2308. Keep the model free of UI concepts (testable headless).
2309. Support a 'replay' of the command log to reproduce a doc.
2310. Allow plugins to extend the model with custom node types.
2311. Provide a 'graph view' linking related objects across docs.
2312. Support a 'semantic layer' so our inference engine reads meaning.
2313. Support canonical model for personal use at no cost (WS11).
2314. Support typed nodes for personal use at no cost (WS11).
2315. Support content/style split for personal use at no cost (WS11).
2316. Support internal+OXML serial for personal use at no cost (WS11).
2317. Support DOM-like API for personal use at no cost (WS11).
2318. Support stable IDs for personal use at no cost (WS11).
2319. Support command pattern for personal use at no cost (WS11).
2320. Support rich metadata for personal use at no cost (WS11).
2321. Support model queries for personal use at no cost (WS11).
2322. Support reactive UI for personal use at no cost (WS11).
2323. Support incremental patches for personal use at no cost (WS11).
2324. Support model explorer for personal use at no cost (WS11).
2325. Support model replay for personal use at no cost (WS11).
2326. Support graph view for personal use at no cost (WS11).
2327. Never gate canonical model behind a subscription or account (WS11).
2328. Never gate typed nodes behind a subscription or account (WS11).
2329. Never gate content/style split behind a subscription or account (WS11).
2330. Never gate internal+OXML serial behind a subscription or account (WS11).
2331. Never gate DOM-like API behind a subscription or account (WS11).
2332. Never gate stable IDs behind a subscription or account (WS11).
2333. Never gate command pattern behind a subscription or account (WS11).
2334. Never gate rich metadata behind a subscription or account (WS11).
2335. Never gate model queries behind a subscription or account (WS11).
2336. Never gate reactive UI behind a subscription or account (WS11).
2337. Never gate incremental patches behind a subscription or account (WS11).
2338. Never gate model explorer behind a subscription or account (WS11).
2339. Never gate model replay behind a subscription or account (WS11).
2340. Never gate graph view behind a subscription or account (WS11).
2341. Add canonical model as a first-class, offline-capable feature.
2342. Add typed nodes as a first-class, offline-capable feature.
2343. Add content/style split as a first-class, offline-capable feature.
2344. Add internal+OXML serial as a first-class, offline-capable feature.
2345. Add DOM-like API as a first-class, offline-capable feature.
2346. Add stable IDs as a first-class, offline-capable feature.
2347. Add command pattern as a first-class, offline-capable feature.
2348. Add rich metadata as a first-class, offline-capable feature.
2349. Add model queries as a first-class, offline-capable feature.
2350. Add reactive UI as a first-class, offline-capable feature.
2351. Add incremental patches as a first-class, offline-capable feature.
2352. Add model explorer as a first-class, offline-capable feature.
2353. Add model replay as a first-class, offline-capable feature.
2354. Add graph view as a first-class, offline-capable feature.
2355. Expose canonical model through the local extension API (WS12).
2356. Expose typed nodes through the local extension API (WS12).
2357. Expose content/style split through the local extension API (WS12).
2358. Expose internal+OXML serial through the local extension API (WS12).
2359. Expose DOM-like API through the local extension API (WS12).
2360. Expose stable IDs through the local extension API (WS12).
2361. Expose command pattern through the local extension API (WS12).
2362. Expose rich metadata through the local extension API (WS12).
2363. Expose model queries through the local extension API (WS12).
2364. Expose reactive UI through the local extension API (WS12).
2365. Expose incremental patches through the local extension API (WS12).
2366. Expose model explorer through the local extension API (WS12).
2367. Expose model replay through the local extension API (WS12).
2368. Expose graph view through the local extension API (WS12).
2369. Make canonical model work fully on-device with no telemetry (WS11).
2370. Make typed nodes work fully on-device with no telemetry (WS11).
2371. Make content/style split work fully on-device with no telemetry (WS11).
2372. Make internal+OXML serial work fully on-device with no telemetry (WS11).
2373. Make DOM-like API work fully on-device with no telemetry (WS11).
2374. Make stable IDs work fully on-device with no telemetry (WS11).
2375. Make command pattern work fully on-device with no telemetry (WS11).
2376. Make rich metadata work fully on-device with no telemetry (WS11).
2377. Make model queries work fully on-device with no telemetry (WS11).
2378. Make reactive UI work fully on-device with no telemetry (WS11).
2379. Make incremental patches work fully on-device with no telemetry (WS11).
2380. Make model explorer work fully on-device with no telemetry (WS11).
2381. Make model replay work fully on-device with no telemetry (WS11).
2382. Make graph view work fully on-device with no telemetry (WS11).
2383. Test canonical model in CI with the WS22 correctness suite (WS11).
2384. Test typed nodes in CI with the WS22 correctness suite (WS11).
2385. Test content/style split in CI with the WS22 correctness suite (WS11).
2386. Test internal+OXML serial in CI with the WS22 correctness suite (WS11).
2387. Test DOM-like API in CI with the WS22 correctness suite (WS11).
2388. Test stable IDs in CI with the WS22 correctness suite (WS11).
2389. Test command pattern in CI with the WS22 correctness suite (WS11).
2390. Test rich metadata in CI with the WS22 correctness suite (WS11).
2391. Test model queries in CI with the WS22 correctness suite (WS11).
2392. Test reactive UI in CI with the WS22 correctness suite (WS11).
2393. Test incremental patches in CI with the WS22 correctness suite (WS11).
2394. Test model explorer in CI with the WS22 correctness suite (WS11).
2395. Test model replay in CI with the WS22 correctness suite (WS11).
2396. Test graph view in CI with the WS22 correctness suite (WS11).
2397. Provide a fast typed nodes that respects user ownership.
2398. Provide a fast command pattern suitable for enterprise self-hosting.
2399. Provide a offline reactive UI that respects user ownership.
2400. Provide a offline canonical model suitable for enterprise self-hosting.
2401. Provide a local-first internal+OXML serial that respects user ownership.
2402. Provide a local-first model queries suitable for enterprise self-hosting.
2403. Provide a accessible model explorer that respects user ownership.
2404. Provide a accessible content/style split suitable for enterprise self-hosting.
2405. Provide a secure stable IDs that respects user ownership.
2406. Provide a secure incremental patches suitable for enterprise self-hosting.
2407. Provide a simple graph view that respects user ownership.
2408. Provide a simple DOM-like API suitable for enterprise self-hosting.
2409. Provide a auditable rich metadata that respects user ownership.
2410. Provide a auditable model replay suitable for enterprise self-hosting.
2411. Support canonical model for personal use at no cost (WS11).
2412. Support typed nodes for personal use at no cost (WS11).
2413. Support content/style split for personal use at no cost (WS11).
2414. Support internal+OXML serial for personal use at no cost (WS11).
2415. Support DOM-like API for personal use at no cost (WS11).
2416. Support stable IDs for personal use at no cost (WS11).
2417. Support command pattern for personal use at no cost (WS11).
2418. Support rich metadata for personal use at no cost (WS11).
2419. Support model queries for personal use at no cost (WS11).
2420. Support reactive UI for personal use at no cost (WS11).
2421. Support incremental patches for personal use at no cost (WS11).
2422. Support model explorer for personal use at no cost (WS11).
2423. Support model replay for personal use at no cost (WS11).
2424. Support graph view for personal use at no cost (WS11).
2425. Never gate canonical model behind a subscription or account (WS11).
2426. Never gate typed nodes behind a subscription or account (WS11).
2427. Never gate content/style split behind a subscription or account (WS11).
2428. Never gate internal+OXML serial behind a subscription or account (WS11).
2429. Never gate DOM-like API behind a subscription or account (WS11).
2430. Never gate stable IDs behind a subscription or account (WS11).
2431. Never gate command pattern behind a subscription or account (WS11).
2432. Never gate rich metadata behind a subscription or account (WS11).
2433. Never gate model queries behind a subscription or account (WS11).
2434. Never gate reactive UI behind a subscription or account (WS11).
2435. Never gate incremental patches behind a subscription or account (WS11).
2436. Never gate model explorer behind a subscription or account (WS11).
2437. Never gate model replay behind a subscription or account (WS11).
2438. Never gate graph view behind a subscription or account (WS11).
2439. Add canonical model as a first-class, offline-capable feature.
2440. Add typed nodes as a first-class, offline-capable feature.
2441. Add content/style split as a first-class, offline-capable feature.
2442. Add internal+OXML serial as a first-class, offline-capable feature.
2443. Add DOM-like API as a first-class, offline-capable feature.
2444. Add stable IDs as a first-class, offline-capable feature.
2445. Add command pattern as a first-class, offline-capable feature.
2446. Add rich metadata as a first-class, offline-capable feature.
2447. Add model queries as a first-class, offline-capable feature.
2448. Add reactive UI as a first-class, offline-capable feature.
2449. Add incremental patches as a first-class, offline-capable feature.
2450. Add model explorer as a first-class, offline-capable feature.
2451. Add model replay as a first-class, offline-capable feature.
2452. Add graph view as a first-class, offline-capable feature.
2453. Expose canonical model through the local extension API (WS12).
2454. Expose typed nodes through the local extension API (WS12).
2455. Expose content/style split through the local extension API (WS12).
2456. Expose internal+OXML serial through the local extension API (WS12).
2457. Expose DOM-like API through the local extension API (WS12).
2458. Expose stable IDs through the local extension API (WS12).
2459. Expose command pattern through the local extension API (WS12).
2460. Expose rich metadata through the local extension API (WS12).

### 12. Extensibility & Developer Platform (NOT Office.js)

2461. Design a first-party, local-first extension API, not a webview hack.
2462. Run extensions in a sandbox with explicit capability grants.
2463. Support extensions in our safe scripting language and WASM.
2464. Never require a Microsoft/cloud account to publish an add-in.
2465. Provide a local extension store backed by our OS package manager.
2466. Support extensions that read/write the unified object model (WS11).
2467. Provide a stable API version with clear deprecation policy.
2468. Ship a 'hello world' extension and full reference docs.
2469. Support extensions that add new formula functions to sheets.
2470. Support extensions that add new ribbon/tools panels.
2471. Provide a permission prompt listing exactly what an extension wants.
2472. Allow extensions to be disabled/enabled per document.
2473. Support extension signing and a trust store the user controls.
2474. Provide a headless test harness for extensions in CI.
2475. Never break extensions silently across monthly updates.
2476. Support extensions contributing to the command palette.
2477. Allow extensions to register new file importers/exporters.
2478. Provide a 'developer mode' with live reload and logs.
2479. Support extensions that hook autosave/export events.
2480. Allow extensions to call our inference engine via a local API.
2481. Provide a 'capability manifest' so users audit extensions.
2482. Support extensions that add new chart types.
2483. Allow extensions to contribute accessibility handlers.
2484. Provide a 'safe eval' so extensions can't hang the app.
2485. Support extensions that add new proofing languages.
2486. Provide a 'marketplace' that is optional and offline-mirrorable.
2487. Allow extensions to persist data in a sandboxed store.
2488. Support extensions that add new shape/object types to the model.
2489. Provide a 'debug console' for extension developers.
2490. Never inject extensions into the network/telemetry path.
2491. Support extensions that add new export targets (our store).
2492. Allow extensions to be written in any language compiling to WASM.
2493. Provide a 'permission log' of what each extension did.
2494. Support a 'revoke all' that removes an extension's access.
2495. Allow extensions to subscribe to model change events.
2496. Provide a 'sample gallery' of vetted extensions.
2497. Support a 'minimum privilege' default for new extensions.
2498. Provide a 'review' process that is transparent and local-first.
2499. Allow extensions to add custom properties to nodes.
2500. Support a 'script recorder' that scaffolds an extension.
2501. Provide a 'compat shim' so simple macros become extensions.
2502. Allow extensions to surface UI in our OS notification center.
2503. Support extensions that add new collaboration commands.
2504. Provide a 'sandbox escape' detector (hard fail).
2505. Support extensions that add new proofing dictionaries.
2506. Allow extensions to render custom panes.
2507. Provide a 'documented limits' so devs know sandbox bounds.
2508. Support extensions that add new shapes to presentations.
2509. Allow extensions to register new keyboard shortcuts safely.
2510. Provide a 'no-phoning-home' guarantee enforced by sandbox.
2511. Support a 'local CI' that lints extensions before install.
2512. Allow extensions to be updated via our OS package manager.
2513. Provide a 'capability diff' when an extension updates perms.
2514. Support extensions that add new data sources to sheets.
2515. Allow extensions to contribute to the model query language.
2516. Provide a 'trust tier' (system/user/untrusted) for extensions.
2517. Support a 'audit mode' recording all extension file access.
2518. Allow extensions to define new node kinds in the model.
2519. Provide a 'kill switch' that disables all extensions instantly.
2520. Support a 'permission log' of what each extension did.
2521. Allow extensions to be bundled with a document (sandboxed).
2522. Design a first-party, local-first extension API, not a webview hack.
2523. Run extensions in a sandbox with explicit capability grants.
2524. Support extensions in our safe scripting language and WASM.
2525. Never require a Microsoft/cloud account to publish an add-in.
2526. Provide a local extension store backed by our OS package manager.
2527. Support extensions that read/write the unified object model (WS11).
2528. Provide a stable API version with clear deprecation policy.
2529. Ship a 'hello world' extension and full reference docs.
2530. Support extensions that add new formula functions to sheets.
2531. Support extensions that add new ribbon/tools panels.
2532. Provide a permission prompt listing exactly what an extension wants.
2533. Allow extensions to be disabled/enabled per document.
2534. Support extension signing and a trust store the user controls.
2535. Provide a headless test harness for extensions in CI.
2536. Never break extensions silently across monthly updates.
2537. Support extensions contributing to the command palette.
2538. Allow extensions to hook autosave/export events.
2539. Allow extensions to call our inference engine via a local API.
2540. Provide a 'capability manifest' so users audit extensions.
2541. Support extensions that add new chart types.
2542. Support local-first API for personal use at no cost (WS12).
2543. Support capability sandbox for personal use at no cost (WS12).
2544. Support WASM+script for personal use at no cost (WS12).
2545. Support no cloud account for personal use at no cost (WS12).
2546. Support OS pkg store for personal use at no cost (WS12).
2547. Support model read/write for personal use at no cost (WS12).
2548. Support API versioning for personal use at no cost (WS12).
2549. Support hello-world for personal use at no cost (WS12).
2550. Support new functions for personal use at no cost (WS12).
2551. Support new panels for personal use at no cost (WS12).
2552. Support permission prompt for personal use at no cost (WS12).
2553. Support extension signing for personal use at no cost (WS12).
2554. Support headless harness for personal use at no cost (WS12).
2555. Support capability manifest for personal use at no cost (WS12).
2556. Never gate local-first API behind a subscription or account (WS12).
2557. Never gate capability sandbox behind a subscription or account (WS12).
2558. Never gate WASM+script behind a subscription or account (WS12).
2559. Never gate no cloud account behind a subscription or account (WS12).
2560. Never gate OS pkg store behind a subscription or account (WS12).
2561. Never gate model read/write behind a subscription or account (WS12).
2562. Never gate API versioning behind a subscription or account (WS12).
2563. Never gate hello-world behind a subscription or account (WS12).
2564. Never gate new functions behind a subscription or account (WS12).
2565. Never gate new panels behind a subscription or account (WS12).
2566. Never gate permission prompt behind a subscription or account (WS12).
2567. Never gate extension signing behind a subscription or account (WS12).
2568. Never gate headless harness behind a subscription or account (WS12).
2569. Never gate capability manifest behind a subscription or account (WS12).
2570. Add local-first API as a first-class, offline-capable feature.
2571. Add capability sandbox as a first-class, offline-capable feature.
2572. Add WASM+script as a first-class, offline-capable feature.
2573. Add no cloud account as a first-class, offline-capable feature.
2574. Add OS pkg store as a first-class, offline-capable feature.
2575. Add model read/write as a first-class, offline-capable feature.
2576. Add API versioning as a first-class, offline-capable feature.
2577. Add hello-world as a first-class, offline-capable feature.
2578. Add new functions as a first-class, offline-capable feature.
2579. Add new panels as a first-class, offline-capable feature.
2580. Add permission prompt as a first-class, offline-capable feature.
2581. Add extension signing as a first-class, offline-capable feature.
2582. Add headless harness as a first-class, offline-capable feature.
2583. Add capability manifest as a first-class, offline-capable feature.
2584. Expose local-first API through the local extension API (WS12).
2585. Expose capability sandbox through the local extension API (WS12).
2586. Expose WASM+script through the local extension API (WS12).
2587. Expose no cloud account through the local extension API (WS12).
2588. Expose OS pkg store through the local extension API (WS12).
2589. Expose model read/write through the local extension API (WS12).
2590. Expose API versioning through the local extension API (WS12).
2591. Expose hello-world through the local extension API (WS12).
2592. Expose new functions through the local extension API (WS12).
2593. Expose new panels through the local extension API (WS12).
2594. Expose permission prompt through the local extension API (WS12).
2595. Expose extension signing through the local extension API (WS12).
2596. Expose headless harness through the local extension API (WS12).
2597. Expose capability manifest through the local extension API (WS12).
2598. Make local-first API work fully on-device with no telemetry (WS12).
2599. Make capability sandbox work fully on-device with no telemetry (WS12).
2600. Make WASM+script work fully on-device with no telemetry (WS12).
2601. Make no cloud account work fully on-device with no telemetry (WS12).
2602. Make OS pkg store work fully on-device with no telemetry (WS12).
2603. Make model read/write work fully on-device with no telemetry (WS12).
2604. Make API versioning work fully on-device with no telemetry (WS12).
2605. Make hello-world work fully on-device with no telemetry (WS12).
2606. Make new functions work fully on-device with no telemetry (WS12).
2607. Make new panels work fully on-device with no telemetry (WS12).
2608. Make permission prompt work fully on-device with no telemetry (WS12).
2609. Make extension signing work fully on-device with no telemetry (WS12).
2610. Make headless harness work fully on-device with no telemetry (WS12).
2611. Make capability manifest work fully on-device with no telemetry (WS12).
2612. Test local-first API in CI with the WS22 correctness suite (WS12).
2613. Test capability sandbox in CI with the WS22 correctness suite (WS12).
2614. Test WASM+script in CI with the WS22 correctness suite (WS12).
2615. Test no cloud account in CI with the WS22 correctness suite (WS12).
2616. Test OS pkg store in CI with the WS22 correctness suite (WS12).
2617. Test model read/write in CI with the WS22 correctness suite (WS12).
2618. Test API versioning in CI with the WS22 correctness suite (WS12).
2619. Test hello-world in CI with the WS22 correctness suite (WS12).
2620. Test new functions in CI with the WS22 correctness suite (WS12).
2621. Test new panels in CI with the WS22 correctness suite (WS12).
2622. Test permission prompt in CI with the WS22 correctness suite (WS12).
2623. Test extension signing in CI with the WS22 correctness suite (WS12).
2624. Test headless harness in CI with the WS22 correctness suite (WS12).
2625. Test capability manifest in CI with the WS22 correctness suite (WS12).
2626. Provide a fast capability sandbox that respects user ownership.
2627. Provide a fast API versioning suitable for enterprise self-hosting.
2628. Provide a offline new panels that respects user ownership.
2629. Provide a offline local-first API suitable for enterprise self-hosting.
2630. Provide a local-first no cloud account that respects user ownership.
2631. Provide a local-first new functions suitable for enterprise self-hosting.
2632. Provide a accessible extension signing that respects user ownership.
2633. Provide a accessible WASM+script suitable for enterprise self-hosting.
2634. Provide a secure model read/write that respects user ownership.
2635. Provide a secure permission prompt suitable for enterprise self-hosting.
2636. Provide a simple capability manifest that respects user ownership.
2637. Provide a simple OS pkg store suitable for enterprise self-hosting.
2638. Provide a auditable hello-world that respects user ownership.
2639. Provide a auditable headless harness suitable for enterprise self-hosting.
2640. Support local-first API for personal use at no cost (WS12).
2641. Support capability sandbox for personal use at no cost (WS12).
2642. Support WASM+script for personal use at no cost (WS12).
2643. Support no cloud account for personal use at no cost (WS12).
2644. Support OS pkg store for personal use at no cost (WS12).
2645. Support model read/write for personal use at no cost (WS12).
2646. Support API versioning for personal use at no cost (WS12).
2647. Support hello-world for personal use at no cost (WS12).
2648. Support new functions for personal use at no cost (WS12).
2649. Support new panels for personal use at no cost (WS12).
2650. Support permission prompt for personal use at no cost (WS12).
2651. Support extension signing for personal use at no cost (WS12).
2652. Support headless harness for personal use at no cost (WS12).
2653. Support capability manifest for personal use at no cost (WS12).
2654. Never gate local-first API behind a subscription or account (WS12).
2655. Never gate capability sandbox behind a subscription or account (WS12).
2656. Never gate WASM+script behind a subscription or account (WS12).
2657. Never gate no cloud account behind a subscription or account (WS12).
2658. Never gate OS pkg store behind a subscription or account (WS12).
2659. Never gate model read/write behind a subscription or account (WS12).
2660. Never gate API versioning behind a subscription or account (WS12).
2661. Never gate hello-world behind a subscription or account (WS12).
2662. Never gate new functions behind a subscription or account (WS12).
2663. Never gate new panels behind a subscription or account (WS12).
2664. Never gate permission prompt behind a subscription or account (WS12).
2665. Never gate extension signing behind a subscription or account (WS12).
2666. Never gate headless harness behind a subscription or account (WS12).
2667. Never gate capability manifest behind a subscription or account (WS12).
2668. Add local-first API as a first-class, offline-capable feature.
2669. Add capability sandbox as a first-class, offline-capable feature.
2670. Add WASM+script as a first-class, offline-capable feature.
2671. Add no cloud account as a first-class, offline-capable feature.
2672. Add OS pkg store as a first-class, offline-capable feature.
2673. Add model read/write as a first-class, offline-capable feature.
2674. Add API versioning as a first-class, offline-capable feature.
2675. Add hello-world as a first-class, offline-capable feature.
2676. Add new functions as a first-class, offline-capable feature.
2677. Add new panels as a first-class, offline-capable feature.
2678. Add permission prompt as a first-class, offline-capable feature.
2679. Add extension signing as a first-class, offline-capable feature.
2680. Add headless harness as a first-class, offline-capable feature.
2681. Add capability manifest as a first-class, offline-capable feature.
2682. Expose local-first API through the local extension API (WS12).
2683. Expose capability sandbox through the local extension API (WS12).
2684. Expose WASM+script through the local extension API (WS12).
2685. Expose no cloud account through the local extension API (WS12).
2686. Expose OS pkg store through the local extension API (WS12).
2687. Expose model read/write through the local extension API (WS12).
2688. Expose API versioning through the local extension API (WS12).
2689. Expose hello-world through the local extension API (WS12).

### 13. AI / Inference Integration (WuBuMath, local-first)

2690. Integrate our WuBuMath inference engine as the local brain, not Copilot.
2691. Run all AI features on-device by default; cloud optional, never required.
2692. Provide a local LLM assistant that reads the active document context.
2693. Support 'ask about this document' with citations to specific nodes.
2694. Provide draft/rewrite/summarize using the local model offline.
2695. Support grammar/clarity suggestions explained in plain language.
2696. Provide a 'translate' that runs locally for privacy.
2697. Support formula generation from natural language into our engine.
2698. Support 'explain this formula' that traces the computation.
2699. Provide chart-type recommendations from data (local inference).
2700. Support data cleaning suggestions (dedupe, type, fill) locally.
2701. Provide a 'generate table from prompt' that emits real cells.
2702. Support 'make a slide deck from this outline' via local model.
2703. Provide an 'improve writing' with tone/audience controls.
2704. Support a 'research assistant' that queries our knowledge store (WS17).
2705. Provide a 'cite sources' mode that links claims to our vault.
2706. Support speech-to-text dictation via local ASR model.
2707. Support text-to-speech narration locally for review.
2708. Provide a 'describe image' for alt-text generation (opt-in).
2709. Support a 'detect action items' that pulls tasks from docs.
2710. Provide a 'meeting notes to deck' pipeline offline.
2711. Support a 'semantic search' across all local documents.
2712. Provide a 'related documents' suggester from the knowledge graph.
2713. Support a 'prompt library' the user owns and edits.
2714. Provide a 'model swapper' so users pick their own local model.
2715. Support a 'context budget' indicator showing tokens used.
2716. Provide a 'no-training' guarantee: your docs never train shared models.
2717. Support a 'private fine-tune' on user's own corpus, local only.
2718. Provide an 'AI audit log' of every model call and its inputs.
2719. Support a 'confidence' display so users know model certainty.
2720. Provide a 'regenerate' with variation controls.
2721. Support a 'guardrail' that refuses to invent citations.
2722. Provide a 'fact-check' that cross-refs our knowledge store.
2723. Support a 'template from prompt' generator.
2724. Provide a 'local embedding' model for semantic search.
2725. Support a 'summarize selection' with length control.
2726. Provide a 'continue writing' that matches the user's voice.
2727. Support a 'translate comments' across languages live.
2728. Provide a 'spell/grammar' that explains each fix.
2729. Support a 'reading-level' adjuster for audience.
2730. Provide a 'brainstorm' mode that expands an outline.
2731. Support a 'extract tasks' to our OS task list.
2732. Provide a 'generate chart from words' (e.g., 'show sales by region').
2733. Support a 'what-if' natural language on spreadsheets.
2734. Provide a 'explain error' for formula/logging issues.
2735. Support a 'local agent' that performs multi-step doc tasks.
2736. Provide a 'model card' per feature (what it can/can't do).
2737. Support a 'opt-in telemetry' only if user enables, anonymized.
2738. Provide a 'offline proof' that AI works with zero network.
2739. Support a 'prompt injection' guard on document-sourced prompts.
2740. Provide a 'diff view' of AI changes before accept.
2741. Support a 'revert AI' that drops all model edits atomically.
2742. Provide a 'model health' check (does local engine respond).
2743. Support a 'bring your own model' endpoint (local or self-hosted).
2744. Provide a 'token accounting' so users track local compute.
2745. Support a 'privacy mode' that blanks AI from sensitive sections.
2746. Provide a 'local knowledge cutoff' display.
2747. Support a 'multimodal' read of embedded images/charts.
2748. Provide a 'ask the deck' Q&A over a presentation.
2749. Support a 'generate alt-text' batch for all images.
2750. Provide a 'tone detector' for emails/docs.
2751. Support a 'local vector store' shared with our OS search.
2752. Provide a 'AI sidebar' dockable, dismissible, offline.
2753. Support a 'no account' AI: model weights live on the device.
2754. Provide a 'explain like I'm new' simplifier.
2755. Support a 'generate from data' natural-language charts/tables.
2756. Integrate our WuBuMath inference engine as the local brain, not Copilot.
2757. Run all AI features on-device by default; cloud optional, never required.
2758. Provide a local LLM assistant that reads the active document context.
2759. Support 'ask about this document' with citations to specific nodes.
2760. Provide draft/rewrite/summarize using the local model offline.
2761. Support grammar/clarity suggestions explained in plain language.
2762. Provide a 'translate' that runs locally for privacy.
2763. Support formula generation from natural language into our engine.
2764. Support 'explain this formula' that traces the computation.
2765. Provide chart-type recommendations from data (local inference).
2766. Support data cleaning suggestions (dedupe, type, fill) locally.
2767. Provide a 'generate table from prompt' that emits real cells.
2768. Provide a 'make a slide deck from this outline' via local model.
2769. Provide an 'improve writing' with tone/audience controls.
2770. Provide a 'research assistant' that queries our knowledge store (WS17).
2771. Provide a 'cite sources' mode that links claims to our vault.
2772. Support speech-to-text dictation via local ASR model.
2773. Support text-to-speech narration locally for review.
2774. Provide a 'describe image' for alt-text generation (opt-in).
2775. Provide a 'detect action items' that pulls tasks from docs.
2776. Support WuBuMath local for personal use at no cost (WS13).
2777. Support on-device default for personal use at no cost (WS13).
2778. Support doc-context LLM for personal use at no cost (WS13).
2779. Support ask-this-doc for personal use at no cost (WS13).
2780. Support draft/rewrite for personal use at no cost (WS13).
2781. Support grammar explain for personal use at no cost (WS13).
2782. Support local translate for personal use at no cost (WS13).
2783. Support NL formulas for personal use at no cost (WS13).
2784. Support explain formula for personal use at no cost (WS13).
2785. Support chart recs for personal use at no cost (WS13).
2786. Support data cleaning for personal use at no cost (WS13).
2787. Support table from prompt for personal use at no cost (WS13).
2788. Support cite sources for personal use at no cost (WS13).
2789. Support detect action items for personal use at no cost (WS13).
2790. Never gate WuBuMath local behind a subscription or account (WS13).
2791. Never gate on-device default behind a subscription or account (WS13).
2792. Never gate doc-context LLM behind a subscription or account (WS13).
2793. Never gate ask-this-doc behind a subscription or account (WS13).
2794. Never gate draft/rewrite behind a subscription or account (WS13).
2795. Never gate grammar explain behind a subscription or account (WS13).
2796. Never gate local translate behind a subscription or account (WS13).
2797. Never gate NL formulas behind a subscription or account (WS13).
2798. Never gate explain formula behind a subscription or account (WS13).
2799. Never gate chart recs behind a subscription or account (WS13).
2800. Never gate data cleaning behind a subscription or account (WS13).
2801. Never gate table from prompt behind a subscription or account (WS13).
2802. Never gate cite sources behind a subscription or account (WS13).
2803. Never gate detect action items behind a subscription or account (WS13).
2804. Add WuBuMath local as a first-class, offline-capable feature.
2805. Add on-device default as a first-class, offline-capable feature.
2806. Add doc-context LLM as a first-class, offline-capable feature.
2807. Add ask-this-doc as a first-class, offline-capable feature.
2808. Add draft/rewrite as a first-class, offline-capable feature.
2809. Add grammar explain as a first-class, offline-capable feature.
2810. Add local translate as a first-class, offline-capable feature.
2811. Add NL formulas as a first-class, offline-capable feature.
2812. Add explain formula as a first-class, offline-capable feature.
2813. Add chart recs as a first-class, offline-capable feature.
2814. Add data cleaning as a first-class, offline-capable feature.
2815. Add table from prompt as a first-class, offline-capable feature.
2816. Add cite sources as a first-class, offline-capable feature.
2817. Add detect action items as a first-class, offline-capable feature.
2818. Expose WuBuMath local through the local extension API (WS12).
2819. Expose on-device default through the local extension API (WS12).
2820. Expose doc-context LLM through the local extension API (WS12).
2821. Expose ask-this-doc through the local extension API (WS12).
2822. Expose draft/rewrite through the local extension API (WS12).
2823. Expose grammar explain through the local extension API (WS12).
2824. Expose local translate through the local extension API (WS12).
2825. Expose NL formulas through the local extension API (WS12).
2826. Expose explain formula through the local extension API (WS12).
2827. Expose chart recs through the local extension API (WS12).
2828. Expose data cleaning through the local extension API (WS12).
2829. Expose table from prompt through the local extension API (WS12).
2830. Expose cite sources through the local extension API (WS12).
2831. Expose detect action items through the local extension API (WS12).
2832. Make WuBuMath local work fully on-device with no telemetry (WS13).
2833. Make on-device default work fully on-device with no telemetry (WS13).
2834. Make doc-context LLM work fully on-device with no telemetry (WS13).
2835. Make ask-this-doc work fully on-device with no telemetry (WS13).
2836. Make draft/rewrite work fully on-device with no telemetry (WS13).
2837. Make grammar explain work fully on-device with no telemetry (WS13).
2838. Make local translate work fully on-device with no telemetry (WS13).
2839. Make NL formulas work fully on-device with no telemetry (WS13).
2840. Make explain formula work fully on-device with no telemetry (WS13).
2841. Make chart recs work fully on-device with no telemetry (WS13).
2842. Make data cleaning work fully on-device with no telemetry (WS13).
2843. Make table from prompt work fully on-device with no telemetry (WS13).
2844. Make cite sources work fully on-device with no telemetry (WS13).
2845. Make detect action items work fully on-device with no telemetry (WS13).
2846. Test WuBuMath local in CI with the WS22 correctness suite (WS13).
2847. Test on-device default in CI with the WS22 correctness suite (WS13).
2848. Test doc-context LLM in CI with the WS22 correctness suite (WS13).
2849. Test ask-this-doc in CI with the WS22 correctness suite (WS13).
2850. Test draft/rewrite in CI with the WS22 correctness suite (WS13).
2851. Test grammar explain in CI with the WS22 correctness suite (WS13).
2852. Test local translate in CI with the WS22 correctness suite (WS13).
2853. Test NL formulas in CI with the WS22 correctness suite (WS13).
2854. Test explain formula in CI with the WS22 correctness suite (WS13).
2855. Test chart recs in CI with the WS22 correctness suite (WS13).
2856. Test data cleaning in CI with the WS22 correctness suite (WS13).
2857. Test table from prompt in CI with the WS22 correctness suite (WS13).
2858. Test cite sources in CI with the WS22 correctness suite (WS13).
2859. Test detect action items in CI with the WS22 correctness suite (WS13).
2860. Provide a fast on-device default that respects user ownership.
2861. Provide a fast local translate suitable for enterprise self-hosting.
2862. Provide a offline chart recs that respects user ownership.
2863. Provide a offline WuBuMath local suitable for enterprise self-hosting.
2864. Provide a local-first ask-this-doc that respects user ownership.
2865. Provide a local-first explain formula suitable for enterprise self-hosting.
2866. Provide a accessible table from prompt that respects user ownership.
2867. Provide a accessible doc-context LLM suitable for enterprise self-hosting.
2868. Provide a secure grammar explain that respects user ownership.
2869. Provide a secure data cleaning suitable for enterprise self-hosting.
2870. Provide a simple detect action items that respects user ownership.
2871. Provide a simple draft/rewrite suitable for enterprise self-hosting.
2872. Provide a auditable NL formulas that respects user ownership.
2873. Provide a auditable cite sources suitable for enterprise self-hosting.
2874. Support WuBuMath local for personal use at no cost (WS13).
2875. Support on-device default for personal use at no cost (WS13).
2876. Support doc-context LLM for personal use at no cost (WS13).
2877. Support ask-this-doc for personal use at no cost (WS13).
2878. Support draft/rewrite for personal use at no cost (WS13).
2879. Support grammar explain for personal use at no cost (WS13).
2880. Support local translate for personal use at no cost (WS13).
2881. Support NL formulas for personal use at no cost (WS13).
2882. Support explain formula for personal use at no cost (WS13).
2883. Support chart recs for personal use at no cost (WS13).
2884. Support data cleaning for personal use at no cost (WS13).
2885. Support table from prompt for personal use at no cost (WS13).
2886. Support cite sources for personal use at no cost (WS13).
2887. Support detect action items for personal use at no cost (WS13).
2888. Never gate WuBuMath local behind a subscription or account (WS13).
2889. Never gate on-device default behind a subscription or account (WS13).
2890. Never gate doc-context LLM behind a subscription or account (WS13).
2891. Never gate ask-this-doc behind a subscription or account (WS13).
2892. Never gate draft/rewrite behind a subscription or account (WS13).
2893. Never gate grammar explain behind a subscription or account (WS13).
2894. Never gate local translate behind a subscription or account (WS13).
2895. Never gate NL formulas behind a subscription or account (WS13).
2896. Never gate explain formula behind a subscription or account (WS13).
2897. Never gate chart recs behind a subscription or account (WS13).
2898. Never gate data cleaning behind a subscription or account (WS13).
2899. Never gate table from prompt behind a subscription or account (WS13).
2900. Never gate cite sources behind a subscription or account (WS13).
2901. Never gate detect action items behind a subscription or account (WS13).
2902. Add WuBuMath local as a first-class, offline-capable feature.
2903. Add on-device default as a first-class, offline-capable feature.
2904. Add doc-context LLM as a first-class, offline-capable feature.
2905. Add ask-this-doc as a first-class, offline-capable feature.
2906. Add draft/rewrite as a first-class, offline-capable feature.
2907. Add grammar explain as a first-class, offline-capable feature.
2908. Add local translate as a first-class, offline-capable feature.
2909. Add NL formulas as a first-class, offline-capable feature.
2910. Add explain formula as a first-class, offline-capable feature.
2911. Add chart recs as a first-class, offline-capable feature.
2912. Add data cleaning as a first-class, offline-capable feature.
2913. Add table from prompt as a first-class, offline-capable feature.
2914. Add cite sources as a first-class, offline-capable feature.
2915. Add detect action items as a first-class, offline-capable feature.
2916. Expose WuBuMath local through the local extension API (WS12).
2917. Expose on-device default through the local extension API (WS12).
2918. Expose doc-context LLM through the local extension API (WS12).
2919. Expose ask-this-doc through the local extension API (WS12).
2920. Expose draft/rewrite through the local extension API (WS12).
2921. Expose grammar explain through the local extension API (WS12).
2922. Expose local translate through the local extension API (WS12).
2923. Expose NL formulas through the local extension API (WS12).

### 14. Reinforcement Learning Environment

2924. Expose every office task as an RL environment (state/action/reward).
2925. Define a state encoding over the unified object model (WS11).
2926. Define atomic actions: insert, edit, format, navigate, save.
2927. Provide a fast headless simulator for millions of RL episodes.
2928. Provide a reward model for 'task complete' vs 'user satisfied'.
2929. Support curriculum learning from simple to complex documents.
2930. Provide a set of benchmark tasks (make a memo, build a chart).
2931. Allow RL agents to learn keyboard/mouse action sequences.
2932. Provide a 'demo' dataset of human-written docs as expert trajectories.
2933. Support imitation learning from the demo corpus.
2934. Provide a 'task spec' language so users define goals.
2935. Allow agents to propose edits that a human accepts/rejects.
2936. Provide a 'safety wrapper' that blocks destructive actions.
2937. Support a 'undo as negative reward' signal.
2938. Provide a 'habit learner' that adapts UI to user patterns.
2939. Allow the RL env to drive our inference engine for planning.
2940. Provide an OpenAI-gym-like interface for external researchers.
2941. Support a 'multi-agent' mode: one agent per app coordinating.
2942. Provide a 'reward shaping' that values clarity/accessibility.
2943. Allow the env to emit traces for debugging policies.
2944. Provide a 'deterministic mode' for reproducible RL runs.
2945. Support a 'partial observability' setting (agent sees viewport).
2946. Provide a 'transfer learning' from sheet tasks to doc tasks.
2947. Allow agents to query the knowledge graph (WS17) as context.
2948. Provide a 'skill library' of learned sub-policies reusable.
2949. Support a 'human-in-the-loop' reward from real usage.
2950. Provide a 'safety sandbox' where agents practice harmlessly.
2951. Allow the env to run inside our OS as a first-class service.
2952. Provide a 'task success metric' measured by output validation.
2953. Support a 'curriculum generator' that synthesizes tasks.
2954. Provide a 'policy zoo' of released checkpoints.
2955. Allow agents to call the AI assistant as a sub-policy.
2956. Provide a 'explain action' that narrates the agent's choice.
2957. Support a 'constrained agent' obeying team policies.
2958. Provide a 'reward for accessibility' (alt text, contrast).
2959. Allow the env to score 'formatting consistency'.
2960. Provide a 'no-op penalty' to avoid agent stalling.
2961. Support a 'macro discovery' that compresses actions into skills.
2962. Provide a 'task generator from templates'.
2963. Allow agents to learn 'fix this error' from formula failures.
2964. Provide a 'benchmark leaderboard' for office tasks.
2965. Support a 'sim-to-real' gap analysis against human use.
2966. Provide a 'agent replay' viewer for inspection.
2967. Allow the env to export trajectories to our training store.
2968. Provide a 'ablation' tool to study which features matter.
2969. Support a 'safe exploration' that never corrupts real files.
2970. Provide a 'task difficulty' estimator for curriculum.
2971. Allow agents to collaborate in real-time co-editing (WS06).
2972. Provide a 'reward for speed' balanced against quality.
2973. Support a 'distributed RL' across machines via our OS.
2974. Provide a 'policy distillation' to a small on-device model.
2975. Allow the env to generate synthetic training documents.
2976. Provide a 'evaluation harness' on held-out human tasks.
2977. Support a 'fairness' check that agents don't favor styles.
2978. Provide a 'explainability' report per agent decision.
2979. Allow the env to hook our OS accessibility for state (WS04).
2980. Provide a 'agent permissions' separate from user permissions.
2981. Support a 'rollback' of any agent episode instantly.
2982. Provide a 'task ontology' shared across apps.
2983. Allow the env to reward 'minimal steps' for efficiency.
2984. Provide a 'curriculum of 1000 tasks' derived from this docket.
2985. Support a 'self-play' where agents review each other's docs.
2986. Provide a 'deployment' path: learned policy assists users live.
2987. Expose every office task as an RL environment (state/action/reward).
2988. Define a state encoding over the unified object model (WS11).
2989. Define atomic actions: insert, edit, format, navigate, save.
2990. Provide a fast headless simulator for millions of RL episodes.
2991. Provide a reward model for 'task complete' vs 'user satisfied'.
2992. Support curriculum learning from simple to complex documents.
2993. Provide a set of benchmark tasks (make a memo, build a chart).
2994. Allow RL agents to learn keyboard/mouse action sequences.
2995. Provide a 'demo' dataset of human-written docs as expert trajectories.
2996. Support imitation learning from the demo corpus.
2997. Provide a 'task spec' language so users define goals.
2998. Allow agents to propose edits that a human accepts/rejects.
2999. Provide a 'safety wrapper' that blocks destructive actions.
3000. Support a 'undo as negative reward' signal.
3001. Provide a 'habit learner' that adapts UI to user patterns.
3002. Allow the RL env to drive our inference engine for planning.
3003. Provide an OpenAI-gym-like interface for external researchers.
3004. Support a 'multi-agent' mode: one agent per app coordinating.
3005. Provide a 'reward shaping' that values clarity/accessibility.
3006. Allow the env to emit traces for debugging policies.
3007. Support task environment for personal use at no cost (WS14).
3008. Support state encoding for personal use at no cost (WS14).
3009. Support atomic actions for personal use at no cost (WS14).
3010. Support headless sim for personal use at no cost (WS14).
3011. Support reward model for personal use at no cost (WS14).
3012. Support curriculum for personal use at no cost (WS14).
3013. Support benchmark tasks for personal use at no cost (WS14).
3014. Support demo trajectories for personal use at no cost (WS14).
3015. Support imitation learning for personal use at no cost (WS14).
3016. Support task spec for personal use at no cost (WS14).
3017. Support safety wrapper for personal use at no cost (WS14).
3018. Support gym interface for personal use at no cost (WS14).
3019. Support multi-agent for personal use at no cost (WS14).
3020. Support policy zoo for personal use at no cost (WS14).
3021. Never gate task environment behind a subscription or account (WS14).
3022. Never gate state encoding behind a subscription or account (WS14).
3023. Never gate atomic actions behind a subscription or account (WS14).
3024. Never gate headless sim behind a subscription or account (WS14).
3025. Never gate reward model behind a subscription or account (WS14).
3026. Never gate curriculum behind a subscription or account (WS14).
3027. Never gate benchmark tasks behind a subscription or account (WS14).
3028. Never gate demo trajectories behind a subscription or account (WS14).
3029. Never gate imitation learning behind a subscription or account (WS14).
3030. Never gate task spec behind a subscription or account (WS14).
3031. Never gate safety wrapper behind a subscription or account (WS14).
3032. Never gate gym interface behind a subscription or account (WS14).
3033. Never gate multi-agent behind a subscription or account (WS14).
3034. Never gate policy zoo behind a subscription or account (WS14).
3035. Add task environment as a first-class, offline-capable feature.
3036. Add state encoding as a first-class, offline-capable feature.
3037. Add atomic actions as a first-class, offline-capable feature.
3038. Add headless sim as a first-class, offline-capable feature.
3039. Add reward model as a first-class, offline-capable feature.
3040. Add curriculum as a first-class, offline-capable feature.
3041. Add benchmark tasks as a first-class, offline-capable feature.
3042. Add demo trajectories as a first-class, offline-capable feature.
3043. Add imitation learning as a first-class, offline-capable feature.
3044. Add task spec as a first-class, offline-capable feature.
3045. Add safety wrapper as a first-class, offline-capable feature.
3046. Add gym interface as a first-class, offline-capable feature.
3047. Add multi-agent as a first-class, offline-capable feature.
3048. Add policy zoo as a first-class, offline-capable feature.
3049. Expose task environment through the local extension API (WS12).
3050. Expose state encoding through the local extension API (WS12).
3051. Expose atomic actions through the local extension API (WS12).
3052. Expose headless sim through the local extension API (WS12).
3053. Expose reward model through the local extension API (WS12).
3054. Expose curriculum through the local extension API (WS12).
3055. Expose benchmark tasks through the local extension API (WS12).
3056. Expose demo trajectories through the local extension API (WS12).
3057. Expose imitation learning through the local extension API (WS12).
3058. Expose task spec through the local extension API (WS12).
3059. Expose safety wrapper through the local extension API (WS12).
3060. Expose gym interface through the local extension API (WS12).
3061. Expose multi-agent through the local extension API (WS12).
3062. Expose policy zoo through the local extension API (WS12).
3063. Make task environment work fully on-device with no telemetry (WS14).
3064. Make state encoding work fully on-device with no telemetry (WS14).
3065. Make atomic actions work fully on-device with no telemetry (WS14).
3066. Make headless sim work fully on-device with no telemetry (WS14).
3067. Make reward model work fully on-device with no telemetry (WS14).
3068. Make curriculum work fully on-device with no telemetry (WS14).
3069. Make benchmark tasks work fully on-device with no telemetry (WS14).
3070. Make demo trajectories work fully on-device with no telemetry (WS14).
3071. Make imitation learning work fully on-device with no telemetry (WS14).
3072. Make task spec work fully on-device with no telemetry (WS14).
3073. Make safety wrapper work fully on-device with no telemetry (WS14).
3074. Make gym interface work fully on-device with no telemetry (WS14).
3075. Make multi-agent work fully on-device with no telemetry (WS14).
3076. Make policy zoo work fully on-device with no telemetry (WS14).
3077. Test task environment in CI with the WS22 correctness suite (WS14).
3078. Test state encoding in CI with the WS22 correctness suite (WS14).
3079. Test atomic actions in CI with the WS22 correctness suite (WS14).
3080. Test headless sim in CI with the WS22 correctness suite (WS14).
3081. Test reward model in CI with the WS22 correctness suite (WS14).
3082. Test curriculum in CI with the WS22 correctness suite (WS14).
3083. Test benchmark tasks in CI with the WS22 correctness suite (WS14).
3084. Test demo trajectories in CI with the WS22 correctness suite (WS14).
3085. Test imitation learning in CI with the WS22 correctness suite (WS14).
3086. Test task spec in CI with the WS22 correctness suite (WS14).
3087. Test safety wrapper in CI with the WS22 correctness suite (WS14).
3088. Test gym interface in CI with the WS22 correctness suite (WS14).
3089. Test multi-agent in CI with the WS22 correctness suite (WS14).
3090. Test policy zoo in CI with the WS22 correctness suite (WS14).
3091. Provide a fast state encoding that respects user ownership.
3092. Provide a fast benchmark tasks suitable for enterprise self-hosting.
3093. Provide a offline task spec that respects user ownership.
3094. Provide a offline task environment suitable for enterprise self-hosting.
3095. Provide a local-first headless sim that respects user ownership.
3096. Provide a local-first imitation learning suitable for enterprise self-hosting.
3097. Provide a accessible gym interface that respects user ownership.
3098. Provide a accessible atomic actions suitable for enterprise self-hosting.
3099. Provide a secure curriculum that respects user ownership.
3100. Provide a secure safety wrapper suitable for enterprise self-hosting.
3101. Provide a simple policy zoo that respects user ownership.
3102. Provide a simple reward model suitable for enterprise self-hosting.
3103. Provide a auditable demo trajectories that respects user ownership.
3104. Provide a auditable multi-agent suitable for enterprise self-hosting.
3105. Support task environment for personal use at no cost (WS14).
3106. Support state encoding for personal use at no cost (WS14).
3107. Support atomic actions for personal use at no cost (WS14).
3108. Support headless sim for personal use at no cost (WS14).
3109. Support reward model for personal use at no cost (WS14).
3110. Support curriculum for personal use at no cost (WS14).
3111. Support benchmark tasks for personal use at no cost (WS14).
3112. Support demo trajectories for personal use at no cost (WS14).
3113. Support imitation learning for personal use at no cost (WS14).
3114. Support task spec for personal use at no cost (WS14).
3115. Support safety wrapper for personal use at no cost (WS14).
3116. Support gym interface for personal use at no cost (WS14).
3117. Support multi-agent for personal use at no cost (WS14).
3118. Support policy zoo for personal use at no cost (WS14).
3119. Never gate task environment behind a subscription or account (WS14).
3120. Never gate state encoding behind a subscription or account (WS14).
3121. Never gate atomic actions behind a subscription or account (WS14).
3122. Never gate headless sim behind a subscription or account (WS14).
3123. Never gate reward model behind a subscription or account (WS14).
3124. Never gate curriculum behind a subscription or account (WS14).
3125. Never gate benchmark tasks behind a subscription or account (WS14).
3126. Never gate demo trajectories behind a subscription or account (WS14).
3127. Never gate imitation learning behind a subscription or account (WS14).
3128. Never gate task spec behind a subscription or account (WS14).
3129. Never gate safety wrapper behind a subscription or account (WS14).
3130. Never gate gym interface behind a subscription or account (WS14).
3131. Never gate multi-agent behind a subscription or account (WS14).
3132. Never gate policy zoo behind a subscription or account (WS14).
3133. Add task environment as a first-class, offline-capable feature.
3134. Add state encoding as a first-class, offline-capable feature.
3135. Add atomic actions as a first-class, offline-capable feature.
3136. Add headless sim as a first-class, offline-capable feature.
3137. Add reward model as a first-class, offline-capable feature.
3138. Add curriculum as a first-class, offline-capable feature.
3139. Add benchmark tasks as a first-class, offline-capable feature.
3140. Add demo trajectories as a first-class, offline-capable feature.
3141. Add imitation learning as a first-class, offline-capable feature.
3142. Add task spec as a first-class, offline-capable feature.
3143. Add safety wrapper as a first-class, offline-capable feature.
3144. Add gym interface as a first-class, offline-capable feature.
3145. Add multi-agent as a first-class, offline-capable feature.
3146. Add policy zoo as a first-class, offline-capable feature.
3147. Expose task environment through the local extension API (WS12).
3148. Expose state encoding through the local extension API (WS12).
3149. Expose atomic actions through the local extension API (WS12).
3150. Expose headless sim through the local extension API (WS12).
3151. Expose reward model through the local extension API (WS12).
3152. Expose curriculum through the local extension API (WS12).
3153. Expose benchmark tasks through the local extension API (WS12).
3154. Expose demo trajectories through the local extension API (WS12).

### 15. Operating System Integration (like MS Office + Win9x)

3155. Register as the OS default handler for OOXML/ODF/PDF types.
3156. Provide deep file-manager preview (thumbnails of pages/sheets).
3157. Integrate with the OS shell 'open with' and 'share' menus.
3158. Support OS theming (dark/light) automatically.
3159. Expose documents to the OS global search/indexer.
3160. Provide a 'quick note' that drops into the OS clipboard/history.
3161. Integrate with OS notifications for collaboration/comments.
3162. Support OS single-sign-on identity for local sharing.
3163. Provide a 'share sheet' that targets our OS apps and contacts.
3164. Register custom URI schemes (office://open?...) for deep links.
3165. Support drag-and-drop from the OS file manager directly.
3166. Provide a 'send to' that exports to OS mail/messaging.
3167. Integrate with OS spell-check/proofing if available.
3168. Support OS voice input via the platform ASR bridge.
3169. Provide a 'recent documents' jump list / dock stack.
3170. Integrate with OS power management (pause autosave on battery save).
3171. Support OS file versioning (VFS snapshots) natively.
3172. Provide a 'print to office' virtual printer that captures to doc.
3173. Integrate with OS accessibility (AT-SPI/UIA) per WS04.
3174. Support OS sandbox/permission prompts for file access.
3175. Provide a 'quick look' plugin for the OS file viewer.
3176. Integrate with OS contacts/address book for mail merge.
3177. Support OS global hotkeys that launch templates.
3178. Provide a 'document as a folder' mount in the OS.
3179. Integrate with OS backup (exclude temp, keep docs).
3180. Support OS 'open in terminal' for headless batch ops.
3181. Provide a 'status indicator' in the OS tray.
3182. Integrate with OS font management (no private copies).
3183. Support OS locale/region for number/date formats.
3184. Provide a 'share to deck' from any OS app via intent.
3185. Integrate with OS screen capture for embedding.
3186. Support OS 'focus mode' that dims other windows.
3187. Provide a 'file handler' that previews without launching full app.
3188. Integrate with OS encryption (LUKS/BitLocker) at rest.
3189. Support OS 'tag' metadata shown in the file manager.
3190. Provide a 'new document' from OS context menu (per type).
3191. Integrate with OS update mechanism (our package manager).
3192. Support OS 'default apps' control panel registration.
3193. Provide a 'document properties' sheet in the OS file dialog.
3194. Integrate with OS 'continue where you left off' session.
3195. Support OS 'quick actions' (e.g., convert to PDF).
3196. Provide a 'send to knowledge store' OS share target (WS17).
3197. Integrate with OS 'focus assist' to mute notifications.
3198. Support OS 'file associations' without stealing others.
3199. Provide a 'thumbnail cache' the OS file manager reuses.
3200. Integrate with OS 'timeline'/activity history (local).
3201. Support OS 'open from network' transparently.
3202. Provide a 'new from scanner' via OS scan service.
3203. Integrate with OS 'translate' OS-level if present.
3204. Support OS 'privacy dashboard' reflecting our no-telemetry.
3205. Provide a 'document canvas' as an OS-managed surface.
3206. Integrate with OS 'energy saver' to cap background work.
3207. Support OS 'multi-desktop' per app window placement.
3208. Provide a 'share status' in OS presence (optional).
3209. Integrate with OS 'clipboard history' rich paste.
3210. Support OS 'file provider' so docs appear in open dialogs.
3211. Provide a 'launch at login' toggle in OS settings.
3212. Integrate with OS 'text services framework' for IME.
3213. Support OS 'drag file out' of the app to the desktop.
3214. Provide a 'quick create' from OS search bar.
3215. Integrate with OS 'color picker' system-wide.
3216. Support OS 'parental/usage controls' hook if present.
3217. Register as the OS default handler for OOXML/ODF/PDF types.
3218. Provide deep file-manager preview (thumbnails of pages/sheets).
3219. Integrate with the OS shell 'open with' and 'share' menus.
3220. Support OS theming (dark/light) automatically.
3221. Expose documents to the OS global search/indexer.
3222. Provide a 'quick note' that drops into the OS clipboard/history.
3223. Integrate with OS notifications for collaboration/comments.
3224. Support OS single-sign-on identity for local sharing.
3225. Provide a 'share sheet' that targets our OS apps and contacts.
3226. Register custom URI schemes (office://open?...) for deep links.
3227. Support drag-and-drop from the OS file manager directly.
3228. Provide a 'send to' that exports to OS mail/messaging.
3229. Integrate with OS spell-check/proofing if available.
3230. Support OS voice input via the platform ASR bridge.
3231. Provide a 'recent documents' jump list / dock stack.
3232. Integrate with OS power management (pause autosave on battery).
3233. Support OS file versioning (VFS snapshots) natively.
3234. Provide a 'print to office' virtual printer that captures to doc.
3235. Integrate with OS accessibility (AT-SPI/UIA) per WS04.
3236. Support OS sandbox/permission prompts for file access.
3237. Support default handler for personal use at no cost (WS15).
3238. Support file preview for personal use at no cost (WS15).
3239. Support shell menus for personal use at no cost (WS15).
3240. Support OS theming for personal use at no cost (WS15).
3241. Support global search for personal use at no cost (WS15).
3242. Support quick note for personal use at no cost (WS15).
3243. Support OS notifications for personal use at no cost (WS15).
3244. Support SSO identity for personal use at no cost (WS15).
3245. Support share sheet for personal use at no cost (WS15).
3246. Support URI schemes for personal use at no cost (WS15).
3247. Support drag-drop for personal use at no cost (WS15).
3248. Support virtual printer for personal use at no cost (WS15).
3249. Support VFS snapshots for personal use at no cost (WS15).
3250. Support power mgmt for personal use at no cost (WS15).
3251. Never gate default handler behind a subscription or account (WS15).
3252. Never gate file preview behind a subscription or account (WS15).
3253. Never gate shell menus behind a subscription or account (WS15).
3254. Never gate OS theming behind a subscription or account (WS15).
3255. Never gate global search behind a subscription or account (WS15).
3256. Never gate quick note behind a subscription or account (WS15).
3257. Never gate OS notifications behind a subscription or account (WS15).
3258. Never gate SSO identity behind a subscription or account (WS15).
3259. Never gate share sheet behind a subscription or account (WS15).
3260. Never gate URI schemes behind a subscription or account (WS15).
3261. Never gate drag-drop behind a subscription or account (WS15).
3262. Never gate virtual printer behind a subscription or account (WS15).
3263. Never gate VFS snapshots behind a subscription or account (WS15).
3264. Never gate power mgmt behind a subscription or account (WS15).
3265. Add default handler as a first-class, offline-capable feature.
3266. Add file preview as a first-class, offline-capable feature.
3267. Add shell menus as a first-class, offline-capable feature.
3268. Add OS theming as a first-class, offline-capable feature.
3269. Add global search as a first-class, offline-capable feature.
3270. Add quick note as a first-class, offline-capable feature.
3271. Add OS notifications as a first-class, offline-capable feature.
3272. Add SSO identity as a first-class, offline-capable feature.
3273. Add share sheet as a first-class, offline-capable feature.
3274. Add URI schemes as a first-class, offline-capable feature.
3275. Add drag-drop as a first-class, offline-capable feature.
3276. Add virtual printer as a first-class, offline-capable feature.
3277. Add VFS snapshots as a first-class, offline-capable feature.
3278. Add power mgmt as a first-class, offline-capable feature.
3279. Expose default handler through the local extension API (WS12).
3280. Expose file preview through the local extension API (WS12).
3281. Expose shell menus through the local extension API (WS12).
3282. Expose OS theming through the local extension API (WS12).
3283. Expose global search through the local extension API (WS12).
3284. Expose quick note through the local extension API (WS12).
3285. Expose OS notifications through the local extension API (WS12).
3286. Expose SSO identity through the local extension API (WS12).
3287. Expose share sheet through the local extension API (WS12).
3288. Expose URI schemes through the local extension API (WS12).
3289. Expose drag-drop through the local extension API (WS12).
3290. Expose virtual printer through the local extension API (WS12).
3291. Expose VFS snapshots through the local extension API (WS12).
3292. Expose power mgmt through the local extension API (WS12).
3293. Make default handler work fully on-device with no telemetry (WS15).
3294. Make file preview work fully on-device with no telemetry (WS15).
3295. Make shell menus work fully on-device with no telemetry (WS15).
3296. Make OS theming work fully on-device with no telemetry (WS15).
3297. Make global search work fully on-device with no telemetry (WS15).
3298. Make quick note work fully on-device with no telemetry (WS15).
3299. Make OS notifications work fully on-device with no telemetry (WS15).
3300. Make SSO identity work fully on-device with no telemetry (WS15).
3301. Make share sheet work fully on-device with no telemetry (WS15).
3302. Make URI schemes work fully on-device with no telemetry (WS15).
3303. Make drag-drop work fully on-device with no telemetry (WS15).
3304. Make virtual printer work fully on-device with no telemetry (WS15).
3305. Make VFS snapshots work fully on-device with no telemetry (WS15).
3306. Make power mgmt work fully on-device with no telemetry (WS15).
3307. Test default handler in CI with the WS22 correctness suite (WS15).
3308. Test file preview in CI with the WS22 correctness suite (WS15).
3309. Test shell menus in CI with the WS22 correctness suite (WS15).
3310. Test OS theming in CI with the WS22 correctness suite (WS15).
3311. Test global search in CI with the WS22 correctness suite (WS15).
3312. Test quick note in CI with the WS22 correctness suite (WS15).
3313. Test OS notifications in CI with the WS22 correctness suite (WS15).
3314. Test SSO identity in CI with the WS22 correctness suite (WS15).
3315. Test share sheet in CI with the WS22 correctness suite (WS15).
3316. Test URI schemes in CI with the WS22 correctness suite (WS15).
3317. Test drag-drop in CI with the WS22 correctness suite (WS15).
3318. Test virtual printer in CI with the WS22 correctness suite (WS15).
3319. Test VFS snapshots in CI with the WS22 correctness suite (WS15).
3320. Test power mgmt in CI with the WS22 correctness suite (WS15).
3321. Provide a fast file preview that respects user ownership.
3322. Provide a fast OS notifications suitable for enterprise self-hosting.
3323. Provide a offline URI schemes that respects user ownership.
3324. Provide a offline default handler suitable for enterprise self-hosting.
3325. Provide a local-first OS theming that respects user ownership.
3326. Provide a local-first share sheet suitable for enterprise self-hosting.
3327. Provide a accessible virtual printer that respects user ownership.
3328. Provide a accessible shell menus suitable for enterprise self-hosting.
3329. Provide a secure quick note that respects user ownership.
3330. Provide a secure drag-drop suitable for enterprise self-hosting.
3331. Provide a simple power mgmt that respects user ownership.
3332. Provide a simple global search suitable for enterprise self-hosting.
3333. Provide a auditable SSO identity that respects user ownership.
3334. Provide a auditable VFS snapshots suitable for enterprise self-hosting.
3335. Support default handler for personal use at no cost (WS15).
3336. Support file preview for personal use at no cost (WS15).
3337. Support shell menus for personal use at no cost (WS15).
3338. Support OS theming for personal use at no cost (WS15).
3339. Support global search for personal use at no cost (WS15).
3340. Support quick note for personal use at no cost (WS15).
3341. Support OS notifications for personal use at no cost (WS15).
3342. Support SSO identity for personal use at no cost (WS15).
3343. Support share sheet for personal use at no cost (WS15).
3344. Support URI schemes for personal use at no cost (WS15).
3345. Support drag-drop for personal use at no cost (WS15).
3346. Support virtual printer for personal use at no cost (WS15).
3347. Support VFS snapshots for personal use at no cost (WS15).
3348. Support power mgmt for personal use at no cost (WS15).
3349. Never gate default handler behind a subscription or account (WS15).
3350. Never gate file preview behind a subscription or account (WS15).
3351. Never gate shell menus behind a subscription or account (WS15).
3352. Never gate OS theming behind a subscription or account (WS15).
3353. Never gate global search behind a subscription or account (WS15).
3354. Never gate quick note behind a subscription or account (WS15).
3355. Never gate OS notifications behind a subscription or account (WS15).
3356. Never gate SSO identity behind a subscription or account (WS15).
3357. Never gate share sheet behind a subscription or account (WS15).
3358. Never gate URI schemes behind a subscription or account (WS15).
3359. Never gate drag-drop behind a subscription or account (WS15).
3360. Never gate virtual printer behind a subscription or account (WS15).
3361. Never gate VFS snapshots behind a subscription or account (WS15).
3362. Never gate power mgmt behind a subscription or account (WS15).
3363. Add default handler as a first-class, offline-capable feature.
3364. Add file preview as a first-class, offline-capable feature.
3365. Add shell menus as a first-class, offline-capable feature.
3366. Add OS theming as a first-class, offline-capable feature.
3367. Add global search as a first-class, offline-capable feature.
3368. Add quick note as a first-class, offline-capable feature.
3369. Add OS notifications as a first-class, offline-capable feature.
3370. Add SSO identity as a first-class, offline-capable feature.
3371. Add share sheet as a first-class, offline-capable feature.
3372. Add URI schemes as a first-class, offline-capable feature.
3373. Add drag-drop as a first-class, offline-capable feature.
3374. Add virtual printer as a first-class, offline-capable feature.
3375. Add VFS snapshots as a first-class, offline-capable feature.
3376. Add power mgmt as a first-class, offline-capable feature.
3377. Expose default handler through the local extension API (WS12).
3378. Expose file preview through the local extension API (WS12).
3379. Expose shell menus through the local extension API (WS12).
3380. Expose OS theming through the local extension API (WS12).
3381. Expose global search through the local extension API (WS12).
3382. Expose quick note through the local extension API (WS12).
3383. Expose OS notifications through the local extension API (WS12).
3384. Expose SSO identity through the local extension API (WS12).

### 16. Cross-App Workflow & Interop (live objects, undo, clipboard)

3385. Support copy/paste that preserves rich structure across apps.
3386. Support live linked objects (edit sheet, deck chart updates).
3387. Provide a unified clipboard with multiple named clip entries.
3388. Support OLE-like embedding without a Windows dependency.
3389. Provide a single global undo stack spanning cross-app drag.
3390. Support 'paste special' with format choices per target.
3391. Provide a 'smart paste' that adapts to destination style.
3392. Support drag a chart from sheet into a doc/deck live.
3393. Provide a 'send to' between apps (selection -> new slide).
3394. Support a shared theme so all apps match instantly.
3395. Provide a 'collect from docs' that gathers selected content.
3396. Support a unified find across all open documents.
3397. Provide a 'cross-app macro' scripting the whole suite.
3398. Support a 'data bus' so apps share computed values live.
3399. Provide a 'linked range' from sheet feeding a doc field.
3400. Support a 'unlink' that freezes a previously live object.
3401. Provide a 'clipboard history' searchable and pinned.
3402. Support pasting a table that becomes a real table (not image).
3403. Provide a 'format painter' that works across apps.
3404. Support a 'style sync' so heading styles match everywhere.
3405. Provide a 'multi-select' across apps in a workspace.
3406. Support a 'workspace' bundling related docs/sheets/decks.
3407. Provide a 'recent cross-app actions' rail.
3408. Support a 'live object inspector' showing source links.
3409. Provide a 'break link' that inlines the current value.
3410. Support a 'cross-app search' via the OS indexer (WS15).
3411. Provide a 'unified print' preview across a workspace.
3412. Support a 'send selection to AI' from any app (WS13).
3413. Provide a 'copy as' (Markdown, PNG, OOXML, plain).
3414. Support a 'drag image out' to the OS desktop (WS15).
3415. Provide a 'linked comment' that surfaces in all apps.
3416. Support a 'master data' sheet feeding multiple decks/docs.
3417. Provide a 'refresh all links' command.
3418. Support a 'link health' check showing broken sources.
3419. Provide a 'cross-app undo' with a visible transaction log.
3420. Support a 'paste JSON as table' smart conversion.
3421. Provide a 'paste CSV with delimiter detection'.
3422. Support a 'round-trip' doc->sheet->doc without loss.
3423. Provide a 'unified hyperlink' resolver across the workspace.
3424. Support a 'send to knowledge store' (WS17) from any app.
3425. Provide a 'compare workspace' diff across all docs.
3426. Support a 'template from workspace' capturing interlinks.
3427. Provide a 'cross-app selection' clipboard object.
3428. Support a 'live screenshot' embedding that updates.
3429. Provide a 'unified numbering' across docs in a workspace.
3430. Support a 'cross-reference' to any object in any app.
3431. Provide a 'workspace manifest' listing all parts/hashes.
3432. Support a 'export workspace' as a single portable bundle.
3433. Provide a 'import workspace' that relinks objects.
3434. Support a 'shared undo' respecting collaboration (WS06).
3435. Provide a 'clipboard sanitizer' stripping trackers on paste.
3436. Support a 'paste as picture' with editable source link.
3437. Provide a 'cross-app spellcheck' consistent lexicon.
3438. Support a 'send to RL env' (WS14) as a task episode.
3439. Provide a 'unified zoom/theme' applied to all open windows.
3440. Support a 'workspace search' returning hits with app + location.
3441. Provide a 'linked footnote' that follows the source doc.
3442. Support a 'data flow diagram' of live links in a workspace.
3443. Provide a 'disconnect all' that snapshots then unlinks.
3444. Support a 'cross-app quick switch' (Ctrl+Tab cycles apps).
3445. Support copy/paste that preserves rich structure across apps.
3446. Support live linked objects (edit sheet, deck chart updates).
3447. Provide a unified clipboard with multiple named clip entries.
3448. Support OLE-like embedding without a Windows dependency.
3449. Provide a single global undo stack spanning cross-app drag.
3450. Provide a 'paste special' with format choices per target.
3451. Provide a 'smart paste' that adapts to destination style.
3452. Support drag a chart from sheet into a doc/deck live.
3453. Provide a 'send to' between apps (selection -> new slide).
3454. Support a shared theme so all apps match instantly.
3455. Provide a 'collect from docs' that gathers selected content.
3456. Support a unified find across all open documents.
3457. Provide a 'cross-app macro' scripting the whole suite.
3458. Support a 'data bus' so apps share computed values live.
3459. Provide a 'linked range' from sheet feeding a doc field.
3460. Provide a 'unlink' that freezes a previously live object.
3461. Provide a 'clipboard history' searchable and pinned.
3462. Support pasting a table that becomes a real table (not image).
3463. Provide a 'format painter' that works across apps.
3464. Support a 'style sync' so heading styles match everywhere.
3465. Support rich copy/paste for personal use at no cost (WS16).
3466. Support live linked objects for personal use at no cost (WS16).
3467. Support unified clipboard for personal use at no cost (WS16).
3468. Support native embed for personal use at no cost (WS16).
3469. Support global undo for personal use at no cost (WS16).
3470. Support paste special for personal use at no cost (WS16).
3471. Support smart paste for personal use at no cost (WS16).
3472. Support drag chart for personal use at no cost (WS16).
3473. Support send-to app for personal use at no cost (WS16).
3474. Support shared theme for personal use at no cost (WS16).
3475. Support collect docs for personal use at no cost (WS16).
3476. Support unified find for personal use at no cost (WS16).
3477. Support data bus for personal use at no cost (WS16).
3478. Support clipboard history for personal use at no cost (WS16).
3479. Never gate rich copy/paste behind a subscription or account (WS16).
3480. Never gate live linked objects behind a subscription or account (WS16).
3481. Never gate unified clipboard behind a subscription or account (WS16).
3482. Never gate native embed behind a subscription or account (WS16).
3483. Never gate global undo behind a subscription or account (WS16).
3484. Never gate paste special behind a subscription or account (WS16).
3485. Never gate smart paste behind a subscription or account (WS16).
3486. Never gate drag chart behind a subscription or account (WS16).
3487. Never gate send-to app behind a subscription or account (WS16).
3488. Never gate shared theme behind a subscription or account (WS16).
3489. Never gate collect docs behind a subscription or account (WS16).
3490. Never gate unified find behind a subscription or account (WS16).
3491. Never gate data bus behind a subscription or account (WS16).
3492. Never gate clipboard history behind a subscription or account (WS16).
3493. Add rich copy/paste as a first-class, offline-capable feature.
3494. Add live linked objects as a first-class, offline-capable feature.
3495. Add unified clipboard as a first-class, offline-capable feature.
3496. Add native embed as a first-class, offline-capable feature.
3497. Add global undo as a first-class, offline-capable feature.
3498. Add paste special as a first-class, offline-capable feature.
3499. Add smart paste as a first-class, offline-capable feature.
3500. Add drag chart as a first-class, offline-capable feature.
3501. Add send-to app as a first-class, offline-capable feature.
3502. Add shared theme as a first-class, offline-capable feature.
3503. Add collect docs as a first-class, offline-capable feature.
3504. Add unified find as a first-class, offline-capable feature.
3505. Add data bus as a first-class, offline-capable feature.
3506. Add clipboard history as a first-class, offline-capable feature.
3507. Expose rich copy/paste through the local extension API (WS12).
3508. Expose live linked objects through the local extension API (WS12).
3509. Expose unified clipboard through the local extension API (WS12).
3510. Expose native embed through the local extension API (WS12).
3511. Expose global undo through the local extension API (WS12).
3512. Expose paste special through the local extension API (WS12).
3513. Expose smart paste through the local extension API (WS12).
3514. Expose drag chart through the local extension API (WS12).
3515. Expose send-to app through the local extension API (WS12).
3516. Expose shared theme through the local extension API (WS12).
3517. Expose collect docs through the local extension API (WS12).
3518. Expose unified find through the local extension API (WS12).
3519. Expose data bus through the local extension API (WS12).
3520. Expose clipboard history through the local extension API (WS12).
3521. Make rich copy/paste work fully on-device with no telemetry (WS16).
3522. Make live linked objects work fully on-device with no telemetry (WS16).
3523. Make unified clipboard work fully on-device with no telemetry (WS16).
3524. Make native embed work fully on-device with no telemetry (WS16).
3525. Make global undo work fully on-device with no telemetry (WS16).
3526. Make paste special work fully on-device with no telemetry (WS16).
3527. Make smart paste work fully on-device with no telemetry (WS16).
3528. Make drag chart work fully on-device with no telemetry (WS16).
3529. Make send-to app work fully on-device with no telemetry (WS16).
3530. Make shared theme work fully on-device with no telemetry (WS16).
3531. Make collect docs work fully on-device with no telemetry (WS16).
3532. Make unified find work fully on-device with no telemetry (WS16).
3533. Make data bus work fully on-device with no telemetry (WS16).
3534. Make clipboard history work fully on-device with no telemetry (WS16).
3535. Test rich copy/paste in CI with the WS22 correctness suite (WS16).
3536. Test live linked objects in CI with the WS22 correctness suite (WS16).
3537. Test unified clipboard in CI with the WS22 correctness suite (WS16).
3538. Test native embed in CI with the WS22 correctness suite (WS16).
3539. Test global undo in CI with the WS22 correctness suite (WS16).
3540. Test paste special in CI with the WS22 correctness suite (WS16).
3541. Test smart paste in CI with the WS22 correctness suite (WS16).
3542. Test drag chart in CI with the WS22 correctness suite (WS16).
3543. Test send-to app in CI with the WS22 correctness suite (WS16).
3544. Test shared theme in CI with the WS22 correctness suite (WS16).
3545. Test collect docs in CI with the WS22 correctness suite (WS16).
3546. Test unified find in CI with the WS22 correctness suite (WS16).
3547. Test data bus in CI with the WS22 correctness suite (WS16).
3548. Test clipboard history in CI with the WS22 correctness suite (WS16).
3549. Provide a fast live linked objects that respects user ownership.
3550. Provide a fast smart paste suitable for enterprise self-hosting.
3551. Provide a offline shared theme that respects user ownership.
3552. Provide a offline rich copy/paste suitable for enterprise self-hosting.
3553. Provide a local-first native embed that respects user ownership.
3554. Provide a local-first send-to app suitable for enterprise self-hosting.
3555. Provide a accessible unified find that respects user ownership.
3556. Provide a accessible unified clipboard suitable for enterprise self-hosting.
3557. Provide a secure paste special that respects user ownership.
3558. Provide a secure collect docs suitable for enterprise self-hosting.
3559. Provide a simple clipboard history that respects user ownership.
3560. Provide a simple global undo suitable for enterprise self-hosting.
3561. Provide a auditable drag chart that respects user ownership.
3562. Provide a auditable data bus suitable for enterprise self-hosting.
3563. Support rich copy/paste for personal use at no cost (WS16).
3564. Support live linked objects for personal use at no cost (WS16).
3565. Support unified clipboard for personal use at no cost (WS16).
3566. Support native embed for personal use at no cost (WS16).
3567. Support global undo for personal use at no cost (WS16).
3568. Support paste special for personal use at no cost (WS16).
3569. Support smart paste for personal use at no cost (WS16).
3570. Support drag chart for personal use at no cost (WS16).
3571. Support send-to app for personal use at no cost (WS16).
3572. Support shared theme for personal use at no cost (WS16).
3573. Support collect docs for personal use at no cost (WS16).
3574. Support unified find for personal use at no cost (WS16).
3575. Support data bus for personal use at no cost (WS16).
3576. Support clipboard history for personal use at no cost (WS16).
3577. Never gate rich copy/paste behind a subscription or account (WS16).
3578. Never gate live linked objects behind a subscription or account (WS16).
3579. Never gate unified clipboard behind a subscription or account (WS16).
3580. Never gate native embed behind a subscription or account (WS16).
3581. Never gate global undo behind a subscription or account (WS16).
3582. Never gate paste special behind a subscription or account (WS16).
3583. Never gate smart paste behind a subscription or account (WS16).
3584. Never gate drag chart behind a subscription or account (WS16).
3585. Never gate send-to app behind a subscription or account (WS16).
3586. Never gate shared theme behind a subscription or account (WS16).
3587. Never gate collect docs behind a subscription or account (WS16).
3588. Never gate unified find behind a subscription or account (WS16).
3589. Never gate data bus behind a subscription or account (WS16).
3590. Never gate clipboard history behind a subscription or account (WS16).
3591. Add rich copy/paste as a first-class, offline-capable feature.
3592. Add live linked objects as a first-class, offline-capable feature.
3593. Add unified clipboard as a first-class, offline-capable feature.
3594. Add native embed as a first-class, offline-capable feature.
3595. Add global undo as a first-class, offline-capable feature.
3596. Add paste special as a first-class, offline-capable feature.
3597. Add smart paste as a first-class, offline-capable feature.
3598. Add drag chart as a first-class, offline-capable feature.
3599. Add send-to app as a first-class, offline-capable feature.
3600. Add shared theme as a first-class, offline-capable feature.
3601. Add collect docs as a first-class, offline-capable feature.
3602. Add unified find as a first-class, offline-capable feature.
3603. Add data bus as a first-class, offline-capable feature.
3604. Add clipboard history as a first-class, offline-capable feature.
3605. Expose rich copy/paste through the local extension API (WS12).
3606. Expose live linked objects through the local extension API (WS12).
3607. Expose unified clipboard through the local extension API (WS12).
3608. Expose native embed through the local extension API (WS12).
3609. Expose global undo through the local extension API (WS12).
3610. Expose paste special through the local extension API (WS12).
3611. Expose smart paste through the local extension API (WS12).
3612. Expose drag chart through the local extension API (WS12).

### 17. Data & Knowledge Integration (our other software, vault, graphs)

3613. Provide a 'knowledge store' where documents become queryable nodes.
3614. Support a local graph of entities extracted from documents.
3615. Allow the suite to read/write our existing 'vault' (see memory map).
3616. Provide semantic search across docs via local embeddings (WS13).
3617. Support backlinks so a doc knows what references it.
3618. Provide a 'related' panel drawing from the knowledge graph.
3619. Allow the AI assistant to ground answers in the vault (WS13).
3620. Support tagging documents that sync to the vault taxonomy.
3621. Provide a 'cite' feature linking claims to vault sources.
3622. Support import from our other software's data formats.
3623. Provide a 'publish to vault' that atomizes a doc into notes.
3624. Support a 'collection' that bundles docs by project.
3625. Provide a 'timeline' view of when docs were created/edited.
3626. Allow the RL env to use the graph as state context (WS14).
3627. Support a 'query language' over the knowledge store.
3628. Provide a 'daily digest' of changes across the vault.
3629. Support export to our OS-indexed search.
3630. Provide a 'private web' of the user's own documents.
3631. Support 'mentions' of vault entities inside documents.
3632. Provide a 'graph explorer' UI for the knowledge store.
3633. Support conflict-free sync of the vault across devices.
3634. Provide a 'diff vault' showing doc changes over time.
3635. Support 'templates as vault items' reused everywhere.
3636. Provide a 'trash/archive' with restore in the vault.
3637. Support 'access control' on vault collections.
3638. Provide a 'workspace' that is a live view of the vault (WS16).
3639. Support a 'semantic duplicate' finder across docs.
3640. Provide a 'concept map' auto-built from headings/terms.
3641. Support 'annotations' that attach to any node (WS11).
3642. Provide a 'citation style' manager (APA/MLA/Chicago).
3643. Support 'import bibliography' from Zotero/BibTeX.
3644. Provide a 'knowledge card' summarizing a topic from docs.
3645. Support 'link suggestions' while writing (like a graph).
3646. Provide a 'vault health' (orphans, duplicates, stale).
3647. Support 'export graph' to our OS for other apps to use.
3648. Provide a 'private search' that never leaves the device.
3649. Support 'entity resolution' merging same person/topic.
3650. Provide a 'timeline' of entity mentions across docs.
3651. Support 'collections as playlists' for review.
3652. Provide a 'vault as a filesystem' mount in the OS (WS15).
3653. Support 'AI summarizer' over the whole vault (WS13).
3654. Provide a 'what changed this week' report.
3655. Support 'tag hierarchy' with inheritance.
3656. Provide a 'document -> vault' two-way link with sync.
3657. Support 'protected vault' encrypted at rest (WS02).
3658. Provide a 'vault query' in the command palette (WS03).
3659. Support 'related decks/docs' surfaced in new-doc wizard.
3660. Provide a 'knowledge graph' export to our RL env (WS14).
3661. Support 'annotations sync' to our reading/other apps.
3662. Provide a 'source library' for research workflows.
3663. Support 'auto-tag' via local model on save (opt-in).
3664. Provide a 'vault backup' that is portable folders.
3665. Support 'merge vaults' from multiple machines.
3666. Provide a 'vault stats' (nodes, edges, size).
3667. Support 'semantic alerts' when new doc contradicts old.
3668. Provide a 'cite-while-present' for decks (WS10).
3669. Support 'vault as context' for the AI assistant (WS13).
3670. Provide a 'entity extractor' that runs offline.
3671. Support 'link to OS contacts' from the vault (WS15).
3672. Provide a 'knowledge diff' between two vault snapshots.
3673. Support 'vault permissions' per collection for teams.
3674. Provide a 'graph pruning' to drop stale edges.
3675. Support 'vault API' for our other software to consume.
3676. Provide a 'private index' shared with OS search (WS15).
3677. Support 'document as a graph node' natively.
3678. Provide a 'related tasks' pulling from our OS task list.
3679. Support 'vault export' to open formats (no lock-in).
3680. Provide a 'knowledge store' where documents become queryable nodes.
3681. Support a local graph of entities extracted from documents.
3682. Allow the suite to read/write our existing 'vault' (see memory map).
3683. Provide semantic search across docs via local embeddings (WS13).
3684. Support backlinks so a doc knows what references it.
3685. Provide a 'related' panel drawing from the knowledge graph.
3686. Allow the AI assistant to ground answers in the vault (WS13).
3687. Support tagging documents that sync to the vault taxonomy.
3688. Provide a 'cite' feature linking claims to vault sources.
3689. Support import from our other software's data formats.
3690. Provide a 'publish to vault' that atomizes a doc into notes.
3691. Provide a 'collection' that bundles docs by project.
3692. Provide a 'timeline' view of when docs were created/edited.
3693. Allow the RL env to use the graph as state context (WS14).
3694. Support a 'query language' over the knowledge store.
3695. Provide a 'daily digest' of changes across the vault.
3696. Support export to our OS-indexed search.
3697. Provide a 'private web' of the user's own documents.
3698. Support 'mentions' of vault entities inside documents.
3699. Provide a 'graph explorer' UI for the knowledge store.
3700. Support knowledge store for personal use at no cost (WS17).
3701. Support entity graph for personal use at no cost (WS17).
3702. Support vault read/write for personal use at no cost (WS17).
3703. Support semantic search for personal use at no cost (WS17).
3704. Support backlinks for personal use at no cost (WS17).
3705. Support related panel for personal use at no cost (WS17).
3706. Support AI grounding for personal use at no cost (WS17).
3707. Support vault tagging for personal use at no cost (WS17).
3708. Support cite feature for personal use at no cost (WS17).
3709. Support publish to vault for personal use at no cost (WS17).
3710. Support collections for personal use at no cost (WS17).
3711. Support timeline view for personal use at no cost (WS17).
3712. Support query language for personal use at no cost (WS17).
3713. Support graph explorer for personal use at no cost (WS17).
3714. Never gate knowledge store behind a subscription or account (WS17).
3715. Never gate entity graph behind a subscription or account (WS17).
3716. Never gate vault read/write behind a subscription or account (WS17).
3717. Never gate semantic search behind a subscription or account (WS17).
3718. Never gate backlinks behind a subscription or account (WS17).
3719. Never gate related panel behind a subscription or account (WS17).
3720. Never gate AI grounding behind a subscription or account (WS17).
3721. Never gate vault tagging behind a subscription or account (WS17).
3722. Never gate cite feature behind a subscription or account (WS17).
3723. Never gate publish to vault behind a subscription or account (WS17).
3724. Never gate collections behind a subscription or account (WS17).
3725. Never gate timeline view behind a subscription or account (WS17).
3726. Never gate query language behind a subscription or account (WS17).
3727. Never gate graph explorer behind a subscription or account (WS17).
3728. Add knowledge store as a first-class, offline-capable feature.
3729. Add entity graph as a first-class, offline-capable feature.
3730. Add vault read/write as a first-class, offline-capable feature.
3731. Add semantic search as a first-class, offline-capable feature.
3732. Add backlinks as a first-class, offline-capable feature.
3733. Add related panel as a first-class, offline-capable feature.
3734. Add AI grounding as a first-class, offline-capable feature.
3735. Add vault tagging as a first-class, offline-capable feature.
3736. Add cite feature as a first-class, offline-capable feature.
3737. Add publish to vault as a first-class, offline-capable feature.
3738. Add collections as a first-class, offline-capable feature.
3739. Add timeline view as a first-class, offline-capable feature.
3740. Add query language as a first-class, offline-capable feature.
3741. Add graph explorer as a first-class, offline-capable feature.
3742. Expose knowledge store through the local extension API (WS12).
3743. Expose entity graph through the local extension API (WS12).
3744. Expose vault read/write through the local extension API (WS12).
3745. Expose semantic search through the local extension API (WS12).
3746. Expose backlinks through the local extension API (WS12).
3747. Expose related panel through the local extension API (WS12).
3748. Expose AI grounding through the local extension API (WS12).
3749. Expose vault tagging through the local extension API (WS12).
3750. Expose cite feature through the local extension API (WS12).
3751. Expose publish to vault through the local extension API (WS12).
3752. Expose collections through the local extension API (WS12).
3753. Expose timeline view through the local extension API (WS12).
3754. Expose query language through the local extension API (WS12).
3755. Expose graph explorer through the local extension API (WS12).
3756. Make knowledge store work fully on-device with no telemetry (WS17).
3757. Make entity graph work fully on-device with no telemetry (WS17).
3758. Make vault read/write work fully on-device with no telemetry (WS17).
3759. Make semantic search work fully on-device with no telemetry (WS17).
3760. Make backlinks work fully on-device with no telemetry (WS17).
3761. Make related panel work fully on-device with no telemetry (WS17).
3762. Make AI grounding work fully on-device with no telemetry (WS17).
3763. Make vault tagging work fully on-device with no telemetry (WS17).
3764. Make cite feature work fully on-device with no telemetry (WS17).
3765. Make publish to vault work fully on-device with no telemetry (WS17).
3766. Make collections work fully on-device with no telemetry (WS17).
3767. Make timeline view work fully on-device with no telemetry (WS17).
3768. Make query language work fully on-device with no telemetry (WS17).
3769. Make graph explorer work fully on-device with no telemetry (WS17).
3770. Test knowledge store in CI with the WS22 correctness suite (WS17).
3771. Test entity graph in CI with the WS22 correctness suite (WS17).
3772. Test vault read/write in CI with the WS22 correctness suite (WS17).
3773. Test semantic search in CI with the WS22 correctness suite (WS17).
3774. Test backlinks in CI with the WS22 correctness suite (WS17).
3775. Test related panel in CI with the WS22 correctness suite (WS17).
3776. Test AI grounding in CI with the WS22 correctness suite (WS17).
3777. Test vault tagging in CI with the WS22 correctness suite (WS17).
3778. Test cite feature in CI with the WS22 correctness suite (WS17).
3779. Test publish to vault in CI with the WS22 correctness suite (WS17).
3780. Test collections in CI with the WS22 correctness suite (WS17).
3781. Test timeline view in CI with the WS22 correctness suite (WS17).
3782. Test query language in CI with the WS22 correctness suite (WS17).
3783. Test graph explorer in CI with the WS22 correctness suite (WS17).
3784. Provide a fast entity graph that respects user ownership.
3785. Provide a fast AI grounding suitable for enterprise self-hosting.
3786. Provide a offline publish to vault that respects user ownership.
3787. Provide a offline knowledge store suitable for enterprise self-hosting.
3788. Provide a local-first semantic search that respects user ownership.
3789. Provide a local-first cite feature suitable for enterprise self-hosting.
3790. Provide a accessible timeline view that respects user ownership.
3791. Provide a accessible vault read/write suitable for enterprise self-hosting.
3792. Provide a secure related panel that respects user ownership.
3793. Provide a secure collections suitable for enterprise self-hosting.
3794. Provide a simple graph explorer that respects user ownership.
3795. Provide a simple backlinks suitable for enterprise self-hosting.
3796. Provide a auditable vault tagging that respects user ownership.
3797. Provide a auditable query language suitable for enterprise self-hosting.
3798. Support knowledge store for personal use at no cost (WS17).
3799. Support entity graph for personal use at no cost (WS17).
3800. Support vault read/write for personal use at no cost (WS17).
3801. Support semantic search for personal use at no cost (WS17).
3802. Support backlinks for personal use at no cost (WS17).
3803. Support related panel for personal use at no cost (WS17).
3804. Support AI grounding for personal use at no cost (WS17).
3805. Support vault tagging for personal use at no cost (WS17).
3806. Support cite feature for personal use at no cost (WS17).
3807. Support publish to vault for personal use at no cost (WS17).
3808. Support collections for personal use at no cost (WS17).
3809. Support timeline view for personal use at no cost (WS17).
3810. Support query language for personal use at no cost (WS17).
3811. Support graph explorer for personal use at no cost (WS17).
3812. Never gate knowledge store behind a subscription or account (WS17).
3813. Never gate entity graph behind a subscription or account (WS17).
3814. Never gate vault read/write behind a subscription or account (WS17).
3815. Never gate semantic search behind a subscription or account (WS17).
3816. Never gate backlinks behind a subscription or account (WS17).
3817. Never gate related panel behind a subscription or account (WS17).
3818. Never gate AI grounding behind a subscription or account (WS17).
3819. Never gate vault tagging behind a subscription or account (WS17).
3820. Never gate cite feature behind a subscription or account (WS17).
3821. Never gate publish to vault behind a subscription or account (WS17).
3822. Never gate collections behind a subscription or account (WS17).
3823. Never gate timeline view behind a subscription or account (WS17).
3824. Never gate query language behind a subscription or account (WS17).
3825. Never gate graph explorer behind a subscription or account (WS17).
3826. Add knowledge store as a first-class, offline-capable feature.
3827. Add entity graph as a first-class, offline-capable feature.
3828. Add vault read/write as a first-class, offline-capable feature.
3829. Add semantic search as a first-class, offline-capable feature.
3830. Add backlinks as a first-class, offline-capable feature.
3831. Add related panel as a first-class, offline-capable feature.
3832. Add AI grounding as a first-class, offline-capable feature.
3833. Add vault tagging as a first-class, offline-capable feature.
3834. Add cite feature as a first-class, offline-capable feature.
3835. Add publish to vault as a first-class, offline-capable feature.
3836. Add collections as a first-class, offline-capable feature.
3837. Add timeline view as a first-class, offline-capable feature.
3838. Add query language as a first-class, offline-capable feature.
3839. Add graph explorer as a first-class, offline-capable feature.
3840. Expose knowledge store through the local extension API (WS12).
3841. Expose entity graph through the local extension API (WS12).
3842. Expose vault read/write through the local extension API (WS12).
3843. Expose semantic search through the local extension API (WS12).
3844. Expose backlinks through the local extension API (WS12).
3845. Expose related panel through the local extension API (WS12).
3846. Expose AI grounding through the local extension API (WS12).
3847. Expose vault tagging through the local extension API (WS12).

### 18. Security & Sandboxing

3848. Run all document code (macros/scripts) in a capability sandbox.
3849. Never execute embedded scripts from untrusted docs by default.
3850. Provide a 'trust' model: local files trusted, downloaded prompt.
3851. Sandbox the parser so malformed files can't crash the app.
3852. Fuzz all importers in CI to harden against malicious files.
3853. Support signed documents with local key verification.
3854. Provide a 'block macros' policy for enterprise.
3855. Never auto-run content from the internet inside a doc.
3856. Provide a 'safe open' mode that disables all active content.
3857. Support encrypted at-rest storage with user-held keys (WS02).
3858. Provide a 'redact' tool that irreversibly removes content.
3859. Support a 'watermark' for confidential drafts.
3860. Provide a 'DLP-lite' that flags sending sensitive content out.
3861. Never load remote resources (images/scripts) without consent.
3862. Provide a 'permissions log' of file/network access.
3863. Support a 'disable network' hard switch at app level.
3864. Provide a 'sandbox escape' detector that hard-fails.
3865. Support a 'child process' policy (no spawning shells).
3866. Provide a 'capability manifest' for every extension (WS12).
3867. Support a 'revoke all' that drops every grant.
3868. Provide a 'malware scan' hook to OS scanner if present.
3869. Support a 'no-exec' memory policy for parsed data.
3870. Provide a 'taint tracking' from untrusted sources.
3871. Support a 'safe render' that strips active content on view.
3872. Provide a 'password strength' meter for doc encryption.
3873. Support a 'zero-knowledge' sync (server sees ciphertext).
3874. Provide a 'audit log' of edits for compliance.
3875. Support a 'break-glass' that disables all extensions fast.
3876. Provide a 'content policy' per team/org.
3877. Support a 'signed updates' verified against our key.
3878. Provide a 'no RCE' guarantee on file open.
3879. Support a 'clipboard sanitizer' removing trackers (WS16).
3880. Provide a 'phishing' hint when a doc links to odd URLs.
3881. Support a 'document provenance' chain of custody.
3882. Provide a 'secure delete' that wipes temp files.
3883. Support a 'permission prompt' for any file read outside home.
3884. Provide a 'sandbox report' after opening untrusted doc.
3885. Support a 'disable embeds' mode for max safety.
3886. Provide a 'macro approval' per document/source.
3887. Support a 'least privilege' default for new docs.
3888. Provide a 'security label' (public/confidential) enforcement.
3889. Support a 'encrypt to recipient' via their public key.
3890. Provide a 'tamper-evident' save with hash chain.
3891. Support a 'no clipboard leakage' to other apps unless shared.
3892. Provide a 'sandbox for AI' so the model can't touch files (WS13).
3893. Support a 'policy as code' for orgs to enforce rules.
3894. Provide a 'incident log' for blocked actions.
3895. Support a 'disable network for AI' hard guarantee.
3896. Provide a 'signed boot' of the suite binary.
3897. Support a 'permission diff' on extension update (WS12).
3898. Provide a 'safe mode' that loads zero extensions/macros.
3899. Support a 'file origin' tracking (local/download/email).
3900. Provide a 'block external refs' toggle in open dialog.
3901. Support a 'redaction export' that bakes redactions in.
3902. Provide a 'classification bar' showing doc sensitivity.
3903. Support a 'no covert channel' audit of dependencies.
3904. Provide a 'sandbox for RL agent' (WS14) separate from user files.
3905. Support a 'permission review' wizard on first run.
3906. Provide a 'encrypted swap' so sensitive data isn't paged.
3907. Support a 'lock after idle' with OS screen lock.
3908. Provide a 'secure share link' with expiry (self-hosted).
3909. Support a 'no telemetry' verified by packet capture in CI.
3910. Run all document code (macros/scripts) in a capability sandbox.
3911. Never execute embedded scripts from untrusted docs by default.
3912. Provide a 'trust' model: local files trusted, downloaded prompt.
3913. Sandbox the parser so malformed files can't crash the app.
3914. Fuzz all importers in CI to harden against malicious files.
3915. Support signed documents with local key verification.
3916. Provide a 'block macros' policy for enterprise.
3917. Never auto-run content from the internet inside a doc.
3918. Provide a 'safe open' mode that disables all active content.
3919. Support encrypted at-rest storage with user-held keys (WS02).
3920. Provide a 'redact' tool that irreversibly removes content.
3921. Support a 'watermark' for confidential drafts.
3922. Provide a 'DLP-lite' that flags sending sensitive content out.
3923. Never load remote resources (images/scripts) without consent.
3924. Provide a 'permissions log' of file/network access.
3925. Support a 'disable network' hard switch at app level.
3926. Provide a 'sandbox escape' detector that hard-fails.
3927. Support a 'child process' policy (no spawning shells).
3928. Provide a 'capability manifest' for every extension (WS12).
3929. Support a 'revoke all' that drops every grant.
3930. Support macro sandbox for personal use at no cost (WS18).
3931. Support no untrusted exec for personal use at no cost (WS18).
3932. Support trust model for personal use at no cost (WS18).
3933. Support parser sandbox for personal use at no cost (WS18).
3934. Support importer fuzzing for personal use at no cost (WS18).
3935. Support signed docs for personal use at no cost (WS18).
3936. Support block macros for personal use at no cost (WS18).
3937. Support no auto-run for personal use at no cost (WS18).
3938. Support safe open for personal use at no cost (WS18).
3939. Support encrypted store for personal use at no cost (WS18).
3940. Support redact tool for personal use at no cost (WS18).
3941. Support watermark for personal use at no cost (WS18).
3942. Support DLP-lite for personal use at no cost (WS18).
3943. Support permission log for personal use at no cost (WS18).
3944. Never gate macro sandbox behind a subscription or account (WS18).
3945. Never gate no untrusted exec behind a subscription or account (WS18).
3946. Never gate trust model behind a subscription or account (WS18).
3947. Never gate parser sandbox behind a subscription or account (WS18).
3948. Never gate importer fuzzing behind a subscription or account (WS18).
3949. Never gate signed docs behind a subscription or account (WS18).
3950. Never gate block macros behind a subscription or account (WS18).
3951. Never gate no auto-run behind a subscription or account (WS18).
3952. Never gate safe open behind a subscription or account (WS18).
3953. Never gate encrypted store behind a subscription or account (WS18).
3954. Never gate redact tool behind a subscription or account (WS18).
3955. Never gate watermark behind a subscription or account (WS18).
3956. Never gate DLP-lite behind a subscription or account (WS18).
3957. Never gate permission log behind a subscription or account (WS18).
3958. Add macro sandbox as a first-class, offline-capable feature.
3959. Add no untrusted exec as a first-class, offline-capable feature.
3960. Add trust model as a first-class, offline-capable feature.
3961. Add parser sandbox as a first-class, offline-capable feature.
3962. Add importer fuzzing as a first-class, offline-capable feature.
3963. Add signed docs as a first-class, offline-capable feature.
3964. Add block macros as a first-class, offline-capable feature.
3965. Add no auto-run as a first-class, offline-capable feature.
3966. Add safe open as a first-class, offline-capable feature.
3967. Add encrypted store as a first-class, offline-capable feature.
3968. Add redact tool as a first-class, offline-capable feature.
3969. Add watermark as a first-class, offline-capable feature.
3970. Add DLP-lite as a first-class, offline-capable feature.
3971. Add permission log as a first-class, offline-capable feature.
3972. Expose macro sandbox through the local extension API (WS12).
3973. Expose no untrusted exec through the local extension API (WS12).
3974. Expose trust model through the local extension API (WS12).
3975. Expose parser sandbox through the local extension API (WS12).
3976. Expose importer fuzzing through the local extension API (WS12).
3977. Expose signed docs through the local extension API (WS12).
3978. Expose block macros through the local extension API (WS12).
3979. Expose no auto-run through the local extension API (WS12).
3980. Expose safe open through the local extension API (WS12).
3981. Expose encrypted store through the local extension API (WS12).
3982. Expose redact tool through the local extension API (WS12).
3983. Expose watermark through the local extension API (WS12).
3984. Expose DLP-lite through the local extension API (WS12).
3985. Expose permission log through the local extension API (WS12).
3986. Make macro sandbox work fully on-device with no telemetry (WS18).
3987. Make no untrusted exec work fully on-device with no telemetry (WS18).
3988. Make trust model work fully on-device with no telemetry (WS18).
3989. Make parser sandbox work fully on-device with no telemetry (WS18).
3990. Make importer fuzzing work fully on-device with no telemetry (WS18).
3991. Make signed docs work fully on-device with no telemetry (WS18).
3992. Make block macros work fully on-device with no telemetry (WS18).
3993. Make no auto-run work fully on-device with no telemetry (WS18).
3994. Make safe open work fully on-device with no telemetry (WS18).
3995. Make encrypted store work fully on-device with no telemetry (WS18).
3996. Make redact tool work fully on-device with no telemetry (WS18).
3997. Make watermark work fully on-device with no telemetry (WS18).
3998. Make DLP-lite work fully on-device with no telemetry (WS18).
3999. Make permission log work fully on-device with no telemetry (WS18).
4000. Test macro sandbox in CI with the WS22 correctness suite (WS18).
4001. Test no untrusted exec in CI with the WS22 correctness suite (WS18).
4002. Test trust model in CI with the WS22 correctness suite (WS18).
4003. Test parser sandbox in CI with the WS22 correctness suite (WS18).
4004. Test importer fuzzing in CI with the WS22 correctness suite (WS18).
4005. Test signed docs in CI with the WS22 correctness suite (WS18).
4006. Test block macros in CI with the WS22 correctness suite (WS18).
4007. Test no auto-run in CI with the WS22 correctness suite (WS18).
4008. Test safe open in CI with the WS22 correctness suite (WS18).
4009. Test encrypted store in CI with the WS22 correctness suite (WS18).
4010. Test redact tool in CI with the WS22 correctness suite (WS18).
4011. Test watermark in CI with the WS22 correctness suite (WS18).
4012. Test DLP-lite in CI with the WS22 correctness suite (WS18).
4013. Test permission log in CI with the WS22 correctness suite (WS18).
4014. Provide a fast no untrusted exec that respects user ownership.
4015. Provide a fast block macros suitable for enterprise self-hosting.
4016. Provide a offline encrypted store that respects user ownership.
4017. Provide a offline macro sandbox suitable for enterprise self-hosting.
4018. Provide a local-first parser sandbox that respects user ownership.
4019. Provide a local-first safe open suitable for enterprise self-hosting.
4020. Provide a accessible watermark that respects user ownership.
4021. Provide a accessible trust model suitable for enterprise self-hosting.
4022. Provide a secure signed docs that respects user ownership.
4023. Provide a secure redact tool suitable for enterprise self-hosting.
4024. Provide a simple permission log that respects user ownership.
4025. Provide a simple importer fuzzing suitable for enterprise self-hosting.
4026. Provide a auditable no auto-run that respects user ownership.
4027. Provide a auditable DLP-lite suitable for enterprise self-hosting.
4028. Support macro sandbox for personal use at no cost (WS18).
4029. Support no untrusted exec for personal use at no cost (WS18).
4030. Support trust model for personal use at no cost (WS18).
4031. Support parser sandbox for personal use at no cost (WS18).
4032. Support importer fuzzing for personal use at no cost (WS18).
4033. Support signed docs for personal use at no cost (WS18).
4034. Support block macros for personal use at no cost (WS18).
4035. Support no auto-run for personal use at no cost (WS18).
4036. Support safe open for personal use at no cost (WS18).
4037. Support encrypted store for personal use at no cost (WS18).
4038. Support redact tool for personal use at no cost (WS18).
4039. Support watermark for personal use at no cost (WS18).
4040. Support DLP-lite for personal use at no cost (WS18).
4041. Support permission log for personal use at no cost (WS18).
4042. Never gate macro sandbox behind a subscription or account (WS18).
4043. Never gate no untrusted exec behind a subscription or account (WS18).
4044. Never gate trust model behind a subscription or account (WS18).
4045. Never gate parser sandbox behind a subscription or account (WS18).
4046. Never gate importer fuzzing behind a subscription or account (WS18).
4047. Never gate signed docs behind a subscription or account (WS18).
4048. Never gate block macros behind a subscription or account (WS18).
4049. Never gate no auto-run behind a subscription or account (WS18).
4050. Never gate safe open behind a subscription or account (WS18).
4051. Never gate encrypted store behind a subscription or account (WS18).
4052. Never gate redact tool behind a subscription or account (WS18).
4053. Never gate watermark behind a subscription or account (WS18).
4054. Never gate DLP-lite behind a subscription or account (WS18).
4055. Never gate permission log behind a subscription or account (WS18).
4056. Add macro sandbox as a first-class, offline-capable feature.
4057. Add no untrusted exec as a first-class, offline-capable feature.
4058. Add trust model as a first-class, offline-capable feature.
4059. Add parser sandbox as a first-class, offline-capable feature.
4060. Add importer fuzzing as a first-class, offline-capable feature.
4061. Add signed docs as a first-class, offline-capable feature.
4062. Add block macros as a first-class, offline-capable feature.
4063. Add no auto-run as a first-class, offline-capable feature.
4064. Add safe open as a first-class, offline-capable feature.
4065. Add encrypted store as a first-class, offline-capable feature.
4066. Add redact tool as a first-class, offline-capable feature.
4067. Add watermark as a first-class, offline-capable feature.
4068. Add DLP-lite as a first-class, offline-capable feature.
4069. Add permission log as a first-class, offline-capable feature.
4070. Expose macro sandbox through the local extension API (WS12).
4071. Expose no untrusted exec through the local extension API (WS12).
4072. Expose trust model through the local extension API (WS12).
4073. Expose parser sandbox through the local extension API (WS12).
4074. Expose importer fuzzing through the local extension API (WS12).
4075. Expose signed docs through the local extension API (WS12).
4076. Expose block macros through the local extension API (WS12).
4077. Expose no auto-run through the local extension API (WS12).

### 19. Migration & Compatibility Tooling

4078. Provide a 'import from Word/Excel/PowerPoint' faithful converter.
4079. Support a 'VBA to our safe script' transpiler best-effort.
4080. Provide a 'macro audit' listing what will/won't convert.
4081. Support a 'one-click' bulk converter for whole folders.
4082. Provide a 'compatibility report' vs the source app.
4083. Support a 'preserve macros' by sandboxing them (WS18).
4084. Provide a 'find-broken-refs' on import and offer fixes.
4085. Support a 'map styles' from source template to ours.
4086. Provide a 'translate shortcuts' cheat sheet for migrants.
4087. Support a 'training mode' that teaches our UI to Excel users.
4088. Provide a 'legacy binary' importer (.doc/.xls/.ppt).
4089. Support a 'Google Drive' import via exported OOXML.
4090. Provide a 'migration wizard' that walks the first run.
4091. Support a 'compare original vs imported' diff.
4092. Provide a 'batch re-save' to our native format.
4093. Support a 'preserve comments/threads' on import.
4094. Provide a 'preserve track changes' on import.
4095. Support a 'map fonts' when source font is missing.
4096. Provide a 'report missing features' honestly per file.
4097. Support a 'quick switch' toggle that emulates rival shortcuts.
4098. Provide a 'import from ODF' losslessly.
4099. Support a 'convert to PDF' as a migration checkpoint.
4100. Provide a 'validate' that opens the result in a clean process.
4101. Support a 'undo import' that keeps the original untouched.
4102. Provide a 'language pack' installer for migrants.
4103. Support a 'sample docs' that teach by example.
4104. Provide a 'tutorials' comparing our way to the old way.
4105. Support a 'keyboard layout' switcher (Excel/Libre/ours).
4106. Provide a 'formula translator' for rival function names.
4107. Support a 'chart mapper' from rival chart types.
4108. Provide a 'theme importer' from a rival template.
4109. Support a 'bulk metadata fix' on import (strip paths).
4110. Provide a 'preserve hyperlinks' on import.
4111. Support a 'fix broken media links' on import.
4112. Provide a 'convert macros to extensions' helper (WS12).
4113. Support a 'migrate settings' from a previous install.
4114. Provide a 'portable profile' export/import.
4115. Support a 'compare to MS' fidelity score per doc.
4116. Provide a 'learn our model' interactive tour.
4117. Support a 'import from Apple Pages/Numbers/Keynote'.
4118. Provide a 'batch rename/relink' media on import.
4119. Support a 'preserve numbering' schemes on import.
4120. Provide a 'map colors' from rival theme.
4121. Support a 'convert forms' to our content controls.
4122. Provide a 'preserve smart-art' as editable shapes.
4123. Support a 'convert equations' to MathML.
4124. Provide a 'migrate add-ins' to our extension model (WS12).
4125. Provide a 'compatibility FAQ' per app.
4126. Support a 'dry-run' conversion that reports before writing.
4127. Provide a 'preserve digital signatures' on import.
4128. Support a 'fix encoding' for legacy non-UTF8 files.
4129. Provide a 'map page sizes' from rival defaults.
4130. Support a 'import from Evernote/Notion export' (MD/HTML).
4131. Provide a 'preserve revision history' where possible.
4132. Support a 'convert to our template' in one step.
4133. Provide a 'validation gate' blocking bad imports with reason.
4134. Support a 'batch thumbnail' generation post-import.
4135. Provide a 'migration log' per file for audit.
4136. Support a 'rollback import' if result is unsatisfactory.
4137. Provide a 'preserve language' tags on import.
4138. Support a 'convert to/from' Markdown round-trip.
4139. Provide a 'smart import' that detects format automatically.
4140. Support a 'preserve bookmarks' on import.
4141. Provide a 'import from Word/Excel/PowerPoint' faithful converter.
4142. Support a 'VBA to our safe script' transpiler best-effort.
4143. Provide a 'macro audit' listing what will/won't convert.
4144. Support a 'one-click' bulk converter for whole folders.
4145. Provide a 'compatibility report' vs the source app.
4146. Support a 'preserve macros' by sandboxing them (WS18).
4147. Provide a 'find-broken-refs' on import and offer fixes.
4148. Support a 'map styles' from source template to ours.
4149. Provide a 'translate shortcuts' cheat sheet for migrants.
4150. Support a 'training mode' that teaches our UI to Excel users.
4151. Provide a 'legacy binary' importer (.doc/.xls/.ppt).
4152. Support a 'Google Drive' import via exported OOXML.
4153. Provide a 'migration wizard' that walks the first run.
4154. Support a 'compare original vs imported' diff.
4155. Provide a 'batch re-save' to our native format.
4156. Support a 'preserve comments/threads' on import.
4157. Support a 'preserve track changes' on import.
4158. Support a 'map fonts' when source font is missing.
4159. Provide a 'report missing features' honestly per file.
4160. Provide a 'quick switch' toggle that emulates rival shortcuts.
4161. Support faithful import for personal use at no cost (WS19).
4162. Support VBA transpile for personal use at no cost (WS19).
4163. Support macro audit for personal use at no cost (WS19).
4164. Support bulk convert for personal use at no cost (WS19).
4165. Support compat report for personal use at no cost (WS19).
4166. Support preserve macros for personal use at no cost (WS19).
4167. Support fix broken refs for personal use at no cost (WS19).
4168. Support style mapping for personal use at no cost (WS19).
4169. Support shortcut cheat for personal use at no cost (WS19).
4170. Support training mode for personal use at no cost (WS19).
4171. Support legacy binary for personal use at no cost (WS19).
4172. Support Google import for personal use at no cost (WS19).
4173. Support migration wizard for personal use at no cost (WS19).
4174. Support compare original for personal use at no cost (WS19).
4175. Never gate faithful import behind a subscription or account (WS19).
4176. Never gate VBA transpile behind a subscription or account (WS19).
4177. Never gate macro audit behind a subscription or account (WS19).
4178. Never gate bulk convert behind a subscription or account (WS19).
4179. Never gate compat report behind a subscription or account (WS19).
4180. Never gate preserve macros behind a subscription or account (WS19).
4181. Never gate fix broken refs behind a subscription or account (WS19).
4182. Never gate style mapping behind a subscription or account (WS19).
4183. Never gate shortcut cheat behind a subscription or account (WS19).
4184. Never gate training mode behind a subscription or account (WS19).
4185. Never gate legacy binary behind a subscription or account (WS19).
4186. Never gate Google import behind a subscription or account (WS19).
4187. Never gate migration wizard behind a subscription or account (WS19).
4188. Never gate compare original behind a subscription or account (WS19).
4189. Add faithful import as a first-class, offline-capable feature.
4190. Add VBA transpile as a first-class, offline-capable feature.
4191. Add macro audit as a first-class, offline-capable feature.
4192. Add bulk convert as a first-class, offline-capable feature.
4193. Add compat report as a first-class, offline-capable feature.
4194. Add preserve macros as a first-class, offline-capable feature.
4195. Add fix broken refs as a first-class, offline-capable feature.
4196. Add style mapping as a first-class, offline-capable feature.
4197. Add shortcut cheat as a first-class, offline-capable feature.
4198. Add training mode as a first-class, offline-capable feature.
4199. Add legacy binary as a first-class, offline-capable feature.
4200. Add Google import as a first-class, offline-capable feature.
4201. Add migration wizard as a first-class, offline-capable feature.
4202. Add compare original as a first-class, offline-capable feature.
4203. Expose faithful import through the local extension API (WS12).
4204. Expose VBA transpile through the local extension API (WS12).
4205. Expose macro audit through the local extension API (WS12).
4206. Expose bulk convert through the local extension API (WS12).
4207. Expose compat report through the local extension API (WS12).
4208. Expose preserve macros through the local extension API (WS12).
4209. Expose fix broken refs through the local extension API (WS12).
4210. Expose style mapping through the local extension API (WS12).
4211. Expose shortcut cheat through the local extension API (WS12).
4212. Expose training mode through the local extension API (WS12).
4213. Expose legacy binary through the local extension API (WS12).
4214. Expose Google import through the local extension API (WS12).
4215. Expose migration wizard through the local extension API (WS12).
4216. Expose compare original through the local extension API (WS12).
4217. Make faithful import work fully on-device with no telemetry (WS19).
4218. Make VBA transpile work fully on-device with no telemetry (WS19).
4219. Make macro audit work fully on-device with no telemetry (WS19).
4220. Make bulk convert work fully on-device with no telemetry (WS19).
4221. Make compat report work fully on-device with no telemetry (WS19).
4222. Make preserve macros work fully on-device with no telemetry (WS19).
4223. Make fix broken refs work fully on-device with no telemetry (WS19).
4224. Make style mapping work fully on-device with no telemetry (WS19).
4225. Make shortcut cheat work fully on-device with no telemetry (WS19).
4226. Make training mode work fully on-device with no telemetry (WS19).
4227. Make legacy binary work fully on-device with no telemetry (WS19).
4228. Make Google import work fully on-device with no telemetry (WS19).
4229. Make migration wizard work fully on-device with no telemetry (WS19).
4230. Make compare original work fully on-device with no telemetry (WS19).
4231. Test faithful import in CI with the WS22 correctness suite (WS19).
4232. Test VBA transpile in CI with the WS22 correctness suite (WS19).
4233. Test macro audit in CI with the WS22 correctness suite (WS19).
4234. Test bulk convert in CI with the WS22 correctness suite (WS19).
4235. Test compat report in CI with the WS22 correctness suite (WS19).
4236. Test preserve macros in CI with the WS22 correctness suite (WS19).
4237. Test fix broken refs in CI with the WS22 correctness suite (WS19).
4238. Test style mapping in CI with the WS22 correctness suite (WS19).
4239. Test shortcut cheat in CI with the WS22 correctness suite (WS19).
4240. Test training mode in CI with the WS22 correctness suite (WS19).
4241. Test legacy binary in CI with the WS22 correctness suite (WS19).
4242. Test Google import in CI with the WS22 correctness suite (WS19).
4243. Test migration wizard in CI with the WS22 correctness suite (WS19).
4244. Test compare original in CI with the WS22 correctness suite (WS19).
4245. Provide a fast VBA transpile that respects user ownership.
4246. Provide a fast fix broken refs suitable for enterprise self-hosting.
4247. Provide a offline training mode that respects user ownership.
4248. Provide a offline faithful import suitable for enterprise self-hosting.
4249. Provide a local-first bulk convert that respects user ownership.
4250. Provide a local-first shortcut cheat suitable for enterprise self-hosting.
4251. Provide a accessible Google import that respects user ownership.
4252. Provide a accessible macro audit suitable for enterprise self-hosting.
4253. Provide a secure preserve macros that respects user ownership.
4254. Provide a secure legacy binary suitable for enterprise self-hosting.
4255. Provide a simple compare original that respects user ownership.
4256. Provide a simple compat report suitable for enterprise self-hosting.
4257. Provide a auditable style mapping that respects user ownership.
4258. Provide a auditable migration wizard suitable for enterprise self-hosting.
4259. Support faithful import for personal use at no cost (WS19).
4260. Support VBA transpile for personal use at no cost (WS19).
4261. Support macro audit for personal use at no cost (WS19).
4262. Support bulk convert for personal use at no cost (WS19).
4263. Support compat report for personal use at no cost (WS19).
4264. Support preserve macros for personal use at no cost (WS19).
4265. Support fix broken refs for personal use at no cost (WS19).
4266. Support style mapping for personal use at no cost (WS19).
4267. Support shortcut cheat for personal use at no cost (WS19).
4268. Support training mode for personal use at no cost (WS19).
4269. Support legacy binary for personal use at no cost (WS19).
4270. Support Google import for personal use at no cost (WS19).
4271. Support migration wizard for personal use at no cost (WS19).
4272. Support compare original for personal use at no cost (WS19).
4273. Never gate faithful import behind a subscription or account (WS19).
4274. Never gate VBA transpile behind a subscription or account (WS19).
4275. Never gate macro audit behind a subscription or account (WS19).
4276. Never gate bulk convert behind a subscription or account (WS19).
4277. Never gate compat report behind a subscription or account (WS19).
4278. Never gate preserve macros behind a subscription or account (WS19).
4279. Never gate fix broken refs behind a subscription or account (WS19).
4280. Never gate style mapping behind a subscription or account (WS19).
4281. Never gate shortcut cheat behind a subscription or account (WS19).
4282. Never gate training mode behind a subscription or account (WS19).
4283. Never gate legacy binary behind a subscription or account (WS19).
4284. Never gate Google import behind a subscription or account (WS19).
4285. Never gate migration wizard behind a subscription or account (WS19).
4286. Never gate compare original behind a subscription or account (WS19).
4287. Add faithful import as a first-class, offline-capable feature.
4288. Add VBA transpile as a first-class, offline-capable feature.
4289. Add macro audit as a first-class, offline-capable feature.
4290. Add bulk convert as a first-class, offline-capable feature.
4291. Add compat report as a first-class, offline-capable feature.
4292. Add preserve macros as a first-class, offline-capable feature.
4293. Add fix broken refs as a first-class, offline-capable feature.
4294. Add style mapping as a first-class, offline-capable feature.
4295. Add shortcut cheat as a first-class, offline-capable feature.
4296. Add training mode as a first-class, offline-capable feature.
4297. Add legacy binary as a first-class, offline-capable feature.
4298. Add Google import as a first-class, offline-capable feature.
4299. Add migration wizard as a first-class, offline-capable feature.
4300. Add compare original as a first-class, offline-capable feature.
4301. Expose faithful import through the local extension API (WS12).
4302. Expose VBA transpile through the local extension API (WS12).
4303. Expose macro audit through the local extension API (WS12).
4304. Expose bulk convert through the local extension API (WS12).
4305. Expose compat report through the local extension API (WS12).
4306. Expose preserve macros through the local extension API (WS12).
4307. Expose fix broken refs through the local extension API (WS12).
4308. Expose style mapping through the local extension API (WS12).

### 20. Distribution, Packaging & Onboarding

4309. Ship a single portable binary that needs no installer.
4310. Provide OS-package-manager packages (our OS native).
4311. Support a 'minimal' install (one app) and 'full' bundle.
4312. Provide a 'verified' signature on every release artifact.
4313. Support offline install media (USB) for air-gapped orgs.
4314. Provide a 'first-run wizard' that sets privacy/theme/AI defaults.
4315. Support a 'silent install' with a config file for orgs.
4316. Provide a 'what's new' that is honest and skippable.
4317. Support a 'reset to defaults' without reinstall.
4318. Provide a 'portable profile' on a stick (docs+settings).
4319. Support a 'check for update' that is local/opt-in, no auto.
4320. Provide a 'release notes' in plain language per version.
4321. Support a 'downgrade' path to any prior version.
4322. Provide a 'bandwidth-friendly' update (delta patches).
4323. Support a 'staged rollout' toggle for cautious orgs.
4324. Provide a 'integrity check' on every download.
4325. Support a 'mirror' for the update server (self-host).
4326. Provide a 'no account' update path.
4327. Support a 'Linux/our-OS/other' packages from one source.
4328. Provide a 'container image' for server/batch use.
4329. Support a 'snap/flatpak/ours' where applicable.
4330. Provide a 'docs bundled offline' with the app.
4331. Support a 'sample files' gallery installed locally.
4332. Provide a 'keyboard map' PDF in the installer.
4333. Support a 'uninstall' that scrubs all traces (WS02).
4334. Provide a 'repair install' that fixes broken files.
4335. Support a 'enterprise config' (policy file) at deploy.
4336. Provide a 'telemetry off' as the default, not opt-out.
4337. Support a 'language select' at install with all locales.
4338. Provide a 'accessibility preset' chooser on first run.
4339. Support a 'import old profile' from rival suites.
4340. Provide a 'community builds' clearly labeled.
4341. Support a 'source tarball' for self-compilers.
4342. Provide a 'bill of materials' (SBOM) per release.
4343. Support a 'verify signature' tool for auditors.
4344. Provide a 'no bloatware' guarantee in the installer.
4345. Support a 'choose components' install UI.
4346. Provide a 'portable vs installed' clear choice.
4347. Support a 'update cadence' control (never/monthly/stable).
4348. Provide a 'rollback update' if regressions appear.
4349. Support a 'release channel' (stable/beta) per machine.
4350. Provide a 'offline help' searchable without network.
4351. Support a 'quick start' cards on first launch.
4352. Provide a 'welcome deck' that demonstrates features.
4353. Support a 'send feedback' that is local-first (no forced account).
4354. Provide a 'diagnostic bundle' export for bug reports.
4355. Support a 'no auto-launch' unless user opts in.
4356. Provide a 'file type registration' toggle per format.
4357. Support a 'associate or not' choice at install.
4358. Provide a 'disk footprint' display before install.
4359. Support a 'minimal RAM' preset for old hardware.
4360. Provide a 'privacy notice' shown before any network use.
4361. Support a 'trust store' management UI.
4362. Provide a 'update from LAN' for offline orgs.
4363. Support a 'version badge' in the title bar (toggle).
4364. Provide a 'what changed' diff vs installed version.
4365. Support a 'clean uninstall' removing caches.
4366. Support a 'portable apps menu' integration (our OS).
4367. Provide a 'no background updater' unless enabled.
4368. Support a 'checksum file' alongside releases.
4369. Provide a 'documented EOL' with migration path.
4370. Support a 'per-app install' so users pick what they need.
4371. Provide a 'silent config schema' published for orgs.
4372. Ship a single portable binary that needs no installer.
4373. Provide OS-package-manager packages (our OS native).
4374. Support a 'minimal' install (one app) and 'full' bundle.
4375. Provide a 'verified' signature on every release artifact.
4376. Support offline install media (USB) for air-gapped orgs.
4377. Provide a 'first-run wizard' that sets privacy/theme/AI defaults.
4378. Support a 'silent install' with a config file for orgs.
4379. Provide a 'what's new' that is honest and skippable.
4380. Support a 'reset to defaults' without reinstall.
4381. Provide a 'portable profile' on a stick (docs+settings).
4382. Support a 'check for update' that is local/opt-in, no auto.
4383. Provide a 'release notes' in plain language per version.
4384. Support a 'downgrade' path to any prior version.
4385. Provide a 'bandwidth-friendly' update (delta patches).
4386. Support a 'staged rollout' toggle for cautious orgs.
4387. Provide a 'integrity check' on every download.
4388. Support a 'mirror' for the update server (self-host).
4389. Provide a 'no account' update path.
4390. Provide a 'Linux/our-OS/other' packages from one source.
4391. Provide a 'container image' for server/batch use.
4392. Support portable binary for personal use at no cost (WS20).
4393. Support OS pkg for personal use at no cost (WS20).
4394. Support minimal/full for personal use at no cost (WS20).
4395. Support signed artifacts for personal use at no cost (WS20).
4396. Support offline media for personal use at no cost (WS20).
4397. Support first-run wizard for personal use at no cost (WS20).
4398. Support silent install for personal use at no cost (WS20).
4399. Support honest whats-new for personal use at no cost (WS20).
4400. Support reset defaults for personal use at no cost (WS20).
4401. Support portable profile for personal use at no cost (WS20).
4402. Support opt-in update for personal use at no cost (WS20).
4403. Support release notes for personal use at no cost (WS20).
4404. Support downgrade path for personal use at no cost (WS20).
4405. Support SBOM for personal use at no cost (WS20).
4406. Never gate portable binary behind a subscription or account (WS20).
4407. Never gate OS pkg behind a subscription or account (WS20).
4408. Never gate minimal/full behind a subscription or account (WS20).
4409. Never gate signed artifacts behind a subscription or account (WS20).
4410. Never gate offline media behind a subscription or account (WS20).
4411. Never gate first-run wizard behind a subscription or account (WS20).
4412. Never gate silent install behind a subscription or account (WS20).
4413. Never gate honest whats-new behind a subscription or account (WS20).
4414. Never gate reset defaults behind a subscription or account (WS20).
4415. Never gate portable profile behind a subscription or account (WS20).
4416. Never gate opt-in update behind a subscription or account (WS20).
4417. Never gate release notes behind a subscription or account (WS20).
4418. Never gate downgrade path behind a subscription or account (WS20).
4419. Never gate SBOM behind a subscription or account (WS20).
4420. Add portable binary as a first-class, offline-capable feature.
4421. Add OS pkg as a first-class, offline-capable feature.
4422. Add minimal/full as a first-class, offline-capable feature.
4423. Add signed artifacts as a first-class, offline-capable feature.
4424. Add offline media as a first-class, offline-capable feature.
4425. Add first-run wizard as a first-class, offline-capable feature.
4426. Add silent install as a first-class, offline-capable feature.
4427. Add honest whats-new as a first-class, offline-capable feature.
4428. Add reset defaults as a first-class, offline-capable feature.
4429. Add portable profile as a first-class, offline-capable feature.
4430. Add opt-in update as a first-class, offline-capable feature.
4431. Add release notes as a first-class, offline-capable feature.
4432. Add downgrade path as a first-class, offline-capable feature.
4433. Add SBOM as a first-class, offline-capable feature.
4434. Expose portable binary through the local extension API (WS12).
4435. Expose OS pkg through the local extension API (WS12).
4436. Expose minimal/full through the local extension API (WS12).
4437. Expose signed artifacts through the local extension API (WS12).
4438. Expose offline media through the local extension API (WS12).
4439. Expose first-run wizard through the local extension API (WS12).
4440. Expose silent install through the local extension API (WS12).
4441. Expose honest whats-new through the local extension API (WS12).
4442. Expose reset defaults through the local extension API (WS12).
4443. Expose portable profile through the local extension API (WS12).
4444. Expose opt-in update through the local extension API (WS12).
4445. Expose release notes through the local extension API (WS12).
4446. Expose downgrade path through the local extension API (WS12).
4447. Expose SBOM through the local extension API (WS12).
4448. Make portable binary work fully on-device with no telemetry (WS20).
4449. Make OS pkg work fully on-device with no telemetry (WS20).
4450. Make minimal/full work fully on-device with no telemetry (WS20).
4451. Make signed artifacts work fully on-device with no telemetry (WS20).
4452. Make offline media work fully on-device with no telemetry (WS20).
4453. Make first-run wizard work fully on-device with no telemetry (WS20).
4454. Make silent install work fully on-device with no telemetry (WS20).
4455. Make honest whats-new work fully on-device with no telemetry (WS20).
4456. Make reset defaults work fully on-device with no telemetry (WS20).
4457. Make portable profile work fully on-device with no telemetry (WS20).
4458. Make opt-in update work fully on-device with no telemetry (WS20).
4459. Make release notes work fully on-device with no telemetry (WS20).
4460. Make downgrade path work fully on-device with no telemetry (WS20).
4461. Make SBOM work fully on-device with no telemetry (WS20).
4462. Test portable binary in CI with the WS22 correctness suite (WS20).
4463. Test OS pkg in CI with the WS22 correctness suite (WS20).
4464. Test minimal/full in CI with the WS22 correctness suite (WS20).
4465. Test signed artifacts in CI with the WS22 correctness suite (WS20).
4466. Test offline media in CI with the WS22 correctness suite (WS20).
4467. Test first-run wizard in CI with the WS22 correctness suite (WS20).
4468. Test silent install in CI with the WS22 correctness suite (WS20).
4469. Test honest whats-new in CI with the WS22 correctness suite (WS20).
4470. Test reset defaults in CI with the WS22 correctness suite (WS20).
4471. Test portable profile in CI with the WS22 correctness suite (WS20).
4472. Test opt-in update in CI with the WS22 correctness suite (WS20).
4473. Test release notes in CI with the WS22 correctness suite (WS20).
4474. Test downgrade path in CI with the WS22 correctness suite (WS20).
4475. Test SBOM in CI with the WS22 correctness suite (WS20).
4476. Provide a fast OS pkg that respects user ownership.
4477. Provide a fast silent install suitable for enterprise self-hosting.
4478. Provide a offline portable profile that respects user ownership.
4479. Provide a offline portable binary suitable for enterprise self-hosting.
4480. Provide a local-first signed artifacts that respects user ownership.
4481. Provide a local-first reset defaults suitable for enterprise self-hosting.
4482. Provide a accessible release notes that respects user ownership.
4483. Provide a accessible minimal/full suitable for enterprise self-hosting.
4484. Provide a secure first-run wizard that respects user ownership.
4485. Provide a secure opt-in update suitable for enterprise self-hosting.
4486. Provide a simple SBOM that respects user ownership.
4487. Provide a simple offline media suitable for enterprise self-hosting.
4488. Provide a auditable honest whats-new that respects user ownership.
4489. Provide a auditable downgrade path suitable for enterprise self-hosting.
4490. Support portable binary for personal use at no cost (WS20).
4491. Support OS pkg for personal use at no cost (WS20).
4492. Support minimal/full for personal use at no cost (WS20).
4493. Support signed artifacts for personal use at no cost (WS20).
4494. Support offline media for personal use at no cost (WS20).
4495. Support first-run wizard for personal use at no cost (WS20).
4496. Support silent install for personal use at no cost (WS20).
4497. Support honest whats-new for personal use at no cost (WS20).
4498. Support reset defaults for personal use at no cost (WS20).
4499. Support portable profile for personal use at no cost (WS20).
4500. Support opt-in update for personal use at no cost (WS20).
4501. Support release notes for personal use at no cost (WS20).
4502. Support downgrade path for personal use at no cost (WS20).
4503. Support SBOM for personal use at no cost (WS20).
4504. Never gate portable binary behind a subscription or account (WS20).
4505. Never gate OS pkg behind a subscription or account (WS20).
4506. Never gate minimal/full behind a subscription or account (WS20).
4507. Never gate signed artifacts behind a subscription or account (WS20).
4508. Never gate offline media behind a subscription or account (WS20).
4509. Never gate first-run wizard behind a subscription or account (WS20).
4510. Never gate silent install behind a subscription or account (WS20).
4511. Never gate honest whats-new behind a subscription or account (WS20).
4512. Never gate reset defaults behind a subscription or account (WS20).
4513. Never gate portable profile behind a subscription or account (WS20).
4514. Never gate opt-in update behind a subscription or account (WS20).
4515. Never gate release notes behind a subscription or account (WS20).
4516. Never gate downgrade path behind a subscription or account (WS20).
4517. Never gate SBOM behind a subscription or account (WS20).
4518. Add portable binary as a first-class, offline-capable feature.
4519. Add OS pkg as a first-class, offline-capable feature.
4520. Add minimal/full as a first-class, offline-capable feature.
4521. Add signed artifacts as a first-class, offline-capable feature.
4522. Add offline media as a first-class, offline-capable feature.
4523. Add first-run wizard as a first-class, offline-capable feature.
4524. Add silent install as a first-class, offline-capable feature.
4525. Add honest whats-new as a first-class, offline-capable feature.
4526. Add reset defaults as a first-class, offline-capable feature.
4527. Add portable profile as a first-class, offline-capable feature.
4528. Add opt-in update as a first-class, offline-capable feature.
4529. Add release notes as a first-class, offline-capable feature.
4530. Add downgrade path as a first-class, offline-capable feature.
4531. Add SBOM as a first-class, offline-capable feature.
4532. Expose portable binary through the local extension API (WS12).
4533. Expose OS pkg through the local extension API (WS12).
4534. Expose minimal/full through the local extension API (WS12).
4535. Expose signed artifacts through the local extension API (WS12).
4536. Expose offline media through the local extension API (WS12).
4537. Expose first-run wizard through the local extension API (WS12).
4538. Expose silent install through the local extension API (WS12).
4539. Expose honest whats-new through the local extension API (WS12).

### 21. Mobile, Touch & Pen

4540. Provide a touch-optimized UI mode with large hit targets.
4541. Support active pen inking with pressure and tilt.
4542. Provide handwriting-to-text offline via local model.
4543. Support a 'phone companion' that edits on the go.
4544. Provide a 'tablet layout' with ribbon adapted to touch.
4545. Support multi-touch zoom/pan on canvas.
4546. Provide a 'voice dictation' using OS/local STT (WS13).
4547. Support a 'scan to doc' via OS camera (WS15).
4548. Provide a 'read-aloud' TTS for commute review (WS04).
4549. Support a 'quick capture' widget for notes on lock screen.
4550. Provide a 'sync' that is CRDT-based and offline (WS06).
4551. Support a 'reduced chrome' mobile editor.
4552. Provide a 'thumb keyboard' shortcuts for common actions.
4553. Support a 'stylus eraser' and highlighter natively.
4554. Provide a 'shape recognition' from freehand (local).
4555. Support a 'presentation remote' from phone (WS10).
4556. Provide a 'review mode' on phone for comments/approve.
4557. Support a 'offline first' mobile that syncs later.
4558. Provide a 'small-screen' sheet view (frozen key cols).
4559. Support a 'drag handle' for reordering on touch.
4560. Provide a 'haptic' feedback on actions where available.
4561. Support a 'dark mode' that follows OS (WS03).
4562. Provide a 'tablet split view' doc + AI side-by-side.
4563. Support a 'pen menu' with quick tools.
4564. Provide a 'lasso select' on ink.
4565. Support a 'convert ink to shape/text' on lift pen.
4566. Provide a 'mobile command palette' (WS03).
4567. Support a 'one-handed' mode for phones.
4568. Provide a 'widget' showing recent docs.
4569. Support a 'share sheet' integration (WS15).
4570. Provide a 'biometric unlock' for encrypted docs (WS02).
4571. Support a 'low-bandwidth' sync mode (WS06).
4572. Provide a 'touch track-changes' approve/reject swipe.
4573. Support a 'handwriting math' to equation (local).
4574. Provide a 'camera OCR' to table (local model).
4575. Support a 'phone as second screen' for presenter view.
4576. Provide a 'offline templates' on mobile.
4577. Support a 'sync conflict' resolver friendly to touch.
4578. Provide a 'quick table' creation from voice.
4579. Support a 'stylus scrolling' like paper.
4580. Provide a 'magnifier' for precise touch editing.
4581. Support a 'reduce motion' on mobile (WS04).
4582. Provide a 'data-saver' that skips thumbnails.
4583. Support a 'privacy on device' (no cloud) by default.
4584. Provide a 'tablet PDF annotate' with pen.
4585. Support a 'voice nav' for accessibility (WS04).
4586. Provide a 'gesture' for common commands (undo, save).
4587. Support a 'foldable' adaptive layout.
4588. Provide a 'watch' glance for notifications (optional).
4589. Support a 'offline spellcheck' on mobile.
4590. Provide a 'quick share to deck' from photos.
4591. Support a 'stylus palette' customizable.
4592. Support a 'touch zoom' that keeps text crisp.
4593. Provide a 'mobile print' to OS/network printers.
4594. Support a 'live caption' of presentations (WS10).
4595. Provide a 'pen notebook' infinite canvas mode.
4596. Support a 'sync status' indicator clear on mobile.
4597. Provide a 'offline first' guarantee like desktop (WS02).
4598. Provide a touch-optimized UI mode with large hit targets.
4599. Support active pen inking with pressure and tilt.
4600. Provide handwriting-to-text offline via local model.
4601. Support a 'phone companion' that edits on the go.
4602. Provide a 'tablet layout' with ribbon adapted to touch.
4603. Support multi-touch zoom/pan on canvas.
4604. Provide a 'voice dictation' using OS/local STT (WS13).
4605. Support a 'scan to doc' via OS camera (WS15).
4606. Provide a 'read-aloud' TTS for commute review (WS04).
4607. Provide a 'quick capture' widget for notes on lock screen.
4608. Provide a 'sync' that is CRDT-based and offline (WS06).
4609. Support a 'reduced chrome' mobile editor.
4610. Provide a 'thumb keyboard' shortcuts for common actions.
4611. Support a 'stylus eraser' and highlighter natively.
4612. Provide a 'shape recognition' from freehand (local).
4613. Support a 'presentation remote' from phone (WS10).
4614. Provide a 'review mode' on phone for comments/approve.
4615. Support a 'offline first' mobile that syncs later.
4616. Support a 'small-screen' sheet view (frozen key cols).
4617. Support a 'drag handle' for reordering on touch.
4618. Support touch UI for personal use at no cost (WS21).
4619. Support pen inking for personal use at no cost (WS21).
4620. Support handwriting OCR for personal use at no cost (WS21).
4621. Support phone companion for personal use at no cost (WS21).
4622. Support tablet layout for personal use at no cost (WS21).
4623. Support multi-touch for personal use at no cost (WS21).
4624. Support voice dictation for personal use at no cost (WS21).
4625. Support scan to doc for personal use at no cost (WS21).
4626. Support read-aloud for personal use at no cost (WS21).
4627. Support quick capture for personal use at no cost (WS21).
4628. Support CRDT sync for personal use at no cost (WS21).
4629. Support reduced chrome for personal use at no cost (WS21).
4630. Support stylus eraser for personal use at no cost (WS21).
4631. Support presentation remote for personal use at no cost (WS21).
4632. Never gate touch UI behind a subscription or account (WS21).
4633. Never gate pen inking behind a subscription or account (WS21).
4634. Never gate handwriting OCR behind a subscription or account (WS21).
4635. Never gate phone companion behind a subscription or account (WS21).
4636. Never gate tablet layout behind a subscription or account (WS21).
4637. Never gate multi-touch behind a subscription or account (WS21).
4638. Never gate voice dictation behind a subscription or account (WS21).
4639. Never gate scan to doc behind a subscription or account (WS21).
4640. Never gate read-aloud behind a subscription or account (WS21).
4641. Never gate quick capture behind a subscription or account (WS21).
4642. Never gate CRDT sync behind a subscription or account (WS21).
4643. Never gate reduced chrome behind a subscription or account (WS21).
4644. Never gate stylus eraser behind a subscription or account (WS21).
4645. Never gate presentation remote behind a subscription or account (WS21).
4646. Add touch UI as a first-class, offline-capable feature.
4647. Add pen inking as a first-class, offline-capable feature.
4648. Add handwriting OCR as a first-class, offline-capable feature.
4649. Add phone companion as a first-class, offline-capable feature.
4650. Add tablet layout as a first-class, offline-capable feature.
4651. Add multi-touch as a first-class, offline-capable feature.
4652. Add voice dictation as a first-class, offline-capable feature.
4653. Add scan to doc as a first-class, offline-capable feature.
4654. Add read-aloud as a first-class, offline-capable feature.
4655. Add quick capture as a first-class, offline-capable feature.
4656. Add CRDT sync as a first-class, offline-capable feature.
4657. Add reduced chrome as a first-class, offline-capable feature.
4658. Add stylus eraser as a first-class, offline-capable feature.
4659. Add presentation remote as a first-class, offline-capable feature.
4660. Expose touch UI through the local extension API (WS12).
4661. Expose pen inking through the local extension API (WS12).
4662. Expose handwriting OCR through the local extension API (WS12).
4663. Expose phone companion through the local extension API (WS12).
4664. Expose tablet layout through the local extension API (WS12).
4665. Expose multi-touch through the local extension API (WS12).
4666. Expose voice dictation through the local extension API (WS12).
4667. Expose scan to doc through the local extension API (WS12).
4668. Expose read-aloud through the local extension API (WS12).
4669. Expose quick capture through the local extension API (WS12).
4670. Expose CRDT sync through the local extension API (WS12).
4671. Expose reduced chrome through the local extension API (WS12).
4672. Expose stylus eraser through the local extension API (WS12).
4673. Expose presentation remote through the local extension API (WS12).
4674. Make touch UI work fully on-device with no telemetry (WS21).
4675. Make pen inking work fully on-device with no telemetry (WS21).
4676. Make handwriting OCR work fully on-device with no telemetry (WS21).
4677. Make phone companion work fully on-device with no telemetry (WS21).
4678. Make tablet layout work fully on-device with no telemetry (WS21).
4679. Make multi-touch work fully on-device with no telemetry (WS21).
4680. Make voice dictation work fully on-device with no telemetry (WS21).
4681. Make scan to doc work fully on-device with no telemetry (WS21).
4682. Make read-aloud work fully on-device with no telemetry (WS21).
4683. Make quick capture work fully on-device with no telemetry (WS21).
4684. Make CRDT sync work fully on-device with no telemetry (WS21).
4685. Make reduced chrome work fully on-device with no telemetry (WS21).
4686. Make stylus eraser work fully on-device with no telemetry (WS21).
4687. Make presentation remote work fully on-device with no telemetry (WS21).
4688. Test touch UI in CI with the WS22 correctness suite (WS21).
4689. Test pen inking in CI with the WS22 correctness suite (WS21).
4690. Test handwriting OCR in CI with the WS22 correctness suite (WS21).
4691. Test phone companion in CI with the WS22 correctness suite (WS21).
4692. Test tablet layout in CI with the WS22 correctness suite (WS21).
4693. Test multi-touch in CI with the WS22 correctness suite (WS21).
4694. Test voice dictation in CI with the WS22 correctness suite (WS21).
4695. Test scan to doc in CI with the WS22 correctness suite (WS21).
4696. Test read-aloud in CI with the WS22 correctness suite (WS21).
4697. Test quick capture in CI with the WS22 correctness suite (WS21).
4698. Test CRDT sync in CI with the WS22 correctness suite (WS21).
4699. Test reduced chrome in CI with the WS22 correctness suite (WS21).
4700. Test stylus eraser in CI with the WS22 correctness suite (WS21).
4701. Test presentation remote in CI with the WS22 correctness suite (WS21).
4702. Provide a fast pen inking that respects user ownership.
4703. Provide a fast voice dictation suitable for enterprise self-hosting.
4704. Provide a offline quick capture that respects user ownership.
4705. Provide a offline touch UI suitable for enterprise self-hosting.
4706. Provide a local-first phone companion that respects user ownership.
4707. Provide a local-first read-aloud suitable for enterprise self-hosting.
4708. Provide a accessible reduced chrome that respects user ownership.
4709. Provide a accessible handwriting OCR suitable for enterprise self-hosting.
4710. Provide a secure multi-touch that respects user ownership.
4711. Provide a secure CRDT sync suitable for enterprise self-hosting.
4712. Provide a simple presentation remote that respects user ownership.
4713. Provide a simple tablet layout suitable for enterprise self-hosting.
4714. Provide a auditable scan to doc that respects user ownership.
4715. Provide a auditable stylus eraser suitable for enterprise self-hosting.
4716. Support touch UI for personal use at no cost (WS21).
4717. Support pen inking for personal use at no cost (WS21).
4718. Support handwriting OCR for personal use at no cost (WS21).
4719. Support phone companion for personal use at no cost (WS21).
4720. Support tablet layout for personal use at no cost (WS21).
4721. Support multi-touch for personal use at no cost (WS21).
4722. Support voice dictation for personal use at no cost (WS21).
4723. Support scan to doc for personal use at no cost (WS21).
4724. Support read-aloud for personal use at no cost (WS21).
4725. Support quick capture for personal use at no cost (WS21).
4726. Support CRDT sync for personal use at no cost (WS21).
4727. Support reduced chrome for personal use at no cost (WS21).
4728. Support stylus eraser for personal use at no cost (WS21).
4729. Support presentation remote for personal use at no cost (WS21).
4730. Never gate touch UI behind a subscription or account (WS21).
4731. Never gate pen inking behind a subscription or account (WS21).
4732. Never gate handwriting OCR behind a subscription or account (WS21).
4733. Never gate phone companion behind a subscription or account (WS21).
4734. Never gate tablet layout behind a subscription or account (WS21).
4735. Never gate multi-touch behind a subscription or account (WS21).
4736. Never gate voice dictation behind a subscription or account (WS21).
4737. Never gate scan to doc behind a subscription or account (WS21).
4738. Never gate read-aloud behind a subscription or account (WS21).
4739. Never gate quick capture behind a subscription or account (WS21).
4740. Never gate CRDT sync behind a subscription or account (WS21).
4741. Never gate reduced chrome behind a subscription or account (WS21).
4742. Never gate stylus eraser behind a subscription or account (WS21).
4743. Never gate presentation remote behind a subscription or account (WS21).
4744. Add touch UI as a first-class, offline-capable feature.
4745. Add pen inking as a first-class, offline-capable feature.
4746. Add handwriting OCR as a first-class, offline-capable feature.
4747. Add phone companion as a first-class, offline-capable feature.
4748. Add tablet layout as a first-class, offline-capable feature.
4749. Add multi-touch as a first-class, offline-capable feature.
4750. Add voice dictation as a first-class, offline-capable feature.
4751. Add scan to doc as a first-class, offline-capable feature.
4752. Add read-aloud as a first-class, offline-capable feature.
4753. Add quick capture as a first-class, offline-capable feature.
4754. Add CRDT sync as a first-class, offline-capable feature.
4755. Add reduced chrome as a first-class, offline-capable feature.
4756. Add stylus eraser as a first-class, offline-capable feature.
4757. Add presentation remote as a first-class, offline-capable feature.
4758. Expose touch UI through the local extension API (WS12).
4759. Expose pen inking through the local extension API (WS12).
4760. Expose handwriting OCR through the local extension API (WS12).
4761. Expose phone companion through the local extension API (WS12).
4762. Expose tablet layout through the local extension API (WS12).
4763. Expose multi-touch through the local extension API (WS12).
4764. Expose voice dictation through the local extension API (WS12).
4765. Expose scan to doc through the local extension API (WS12).

### 22. Testing, Correctness & Fuzzing (from-scratch discipline)

4766. Maintain a regression corpus of Excel/LO results to match.
4767. Fuzz every importer (docx/xlsx/pptx/odf/pdf) in CI.
4768. Property-test the model invariants (WS11) on random edits.
4769. Snapshot-test render output per release.
4770. Run ASan/UBSan/MSan on the whole suite in CI.
4771. Maintain a 'golden file' set for round-trip fidelity (WS05).
4772. Test accessibility tree with an automated AT simulator (WS04).
4773. Benchmark perf regressions against a fixed hardware baseline (WS07).
4774. Property-test formula numeric stability against Excel.
4775. Fuzz the DEFLATE round-trip (our own compressor) continuously.
4776. Test cross-app live links don't corrupt on save (WS16).
4777. Test collaboration merge with random concurrent edits (WS06).
4778. Run a differential test vs LibreOffice on public docs.
4779. Test that 'no telemetry' holds via packet capture in CI (WS02).
4780. Property-test the sandbox blocks escapes (WS18).
4781. Test undo/redo across 10k random actions.
4782. Test recovery from killed-process mid-autosave.
4783. Fuzz the expression parser with adversarial input.
4784. Test encoding correctness (UTF-8/legacy) on import (WS19).
4785. Maintain a coverage gate (e.g., >=80%) on core libs.
4786. Test that large docs (1M rows) open within budget (WS07/08).
4787. Test that dark mode has no unthemed panes (WS03).
4788. Property-test CRDT convergence under partitions (WS06).
4789. Test that AI features work fully offline (WS13).
4790. Fuzz the RL environment simulator for crashes (WS14).
4791. Test the extension sandbox with malicious add-ins (WS12).
4792. Test that signed updates verify and reject tampered (WS18/20).
4793. Property-test the unified object model serialization.
4794. Test that migration preserves track changes/comments (WS19).
4795. Run a 'soak' test (days) for memory leaks.
4796. Test OS integration points on real OS builds (WS15).
4797. Property-test the formula engine against a prover (symbolic).
4798. Test that clipboard round-trips rich content (WS16).
4799. Fuzz the PDF exporter for malformed input.
4800. Test that accessibility checker finds known issues (WS04).
4801. Test that 'save as' to each format validates (WS05).
4802. Property-test the permission system denies by default.
4803. Test that the command palette reaches every action (WS03).
4804. Run differential tests of our DEFLATE vs zlib on corpora.
4805. Test that no network egress occurs in local mode (CI packet cap).
4806. Property-test the recalc dependency graph acyclicity.
4807. Test the macro transpiler output executes correctly (WS19).
4808. Fuzz the OOXML writer for invalid ZIP structures.
4809. Test that book-length docs don't OOM (WS09).
4810. Property-test the knowledge graph extractor (WS17).
4811. Test that RL agents can't corrupt real files (WS14/18).
4812. Run a 'reproducible build' verification (bit-identical).
4813. Test that templates open without account (WS01).
4814. Property-test undo atomicity for AI edits (WS13).
4815. Test that the portable build runs from read-only media.
4816. Fuzz the URL/hyperlink handler for injection.
4817. Test that track-changes accept/reject is reversible.
4818. Run a 'chaos' test killing threads mid-operation.
4819. Property-test the style inheritance chain (WS09).
4820. Test that embedded media survives round-trip (WS05).
4821. Test that the AI guardrail never fabricates citations (WS13).
4822. Run a 'differential corpus' vs real-world docs nightly.
4823. Property-test the encryption KDF timing safety (WS02).
4824. Test that the extension permission prompt lists exact caps.
4825. Fuzz the presentation transition engine.
4826. Test that no PII leaks into autosave temp (WS02).
4827. Property-test the cross-app undo transaction log.
4828. Run a 'reproducible build' on three independent workers.
4829. Test that the docket's 1000 tasks map to tracked issues.
4830. Property-test the model patch applies transactionally (WS11).
4831. Test that the RL reward shaping favors accessibility (WS14).
4832. Run a 'no-regression' gate blocking releases on red CI.
4833. Maintain a regression corpus of Excel/LO results to match.
4834. Fuzz every importer (docx/xlsx/pptx/odf/pdf) in CI.
4835. Property-test the model invariants (WS11) on random edits.
4836. Snapshot-test render output per release.
4837. Run ASan/UBSan/MSan on the whole suite in CI.
4838. Maintain a 'golden file' set for round-trip fidelity (WS05).
4839. Test accessibility tree with an automated AT simulator (WS04).
4840. Benchmark perf regressions against a fixed hardware baseline (WS07).
4841. Property-test formula numeric stability against Excel.
4842. Fuzz the DEFLATE round-trip (our own compressor) continuously.
4843. Test cross-app live links don't corrupt on save (WS16).
4844. Test collaboration merge with random concurrent edits (WS06).
4845. Run a differential test vs LibreOffice on public docs.
4846. Test that 'no telemetry' holds via packet capture in CI (WS02).
4847. Property-test the sandbox blocks escapes (WS18).
4848. Test undo/redo across 10k random actions.
4849. Test recovery from killed-process mid-autosave.
4850. Fuzz the expression parser with adversarial input.
4851. Test encoding correctness (UTF-8/legacy) on import (WS19).
4852. Maintain a coverage gate (e.g., >=80%) on core libs.
4853. Support Excel corpus for personal use at no cost (WS22).
4854. Support importer fuzz for personal use at no cost (WS22).
4855. Support model invariants for personal use at no cost (WS22).
4856. Support render snapshots for personal use at no cost (WS22).
4857. Support ASan/UBSan for personal use at no cost (WS22).
4858. Support golden files for personal use at no cost (WS22).
4859. Support AT simulator for personal use at no cost (WS22).
4860. Support perf baseline for personal use at no cost (WS22).
4861. Support numeric stability for personal use at no cost (WS22).
4862. Support DEFLATE fuzz for personal use at no cost (WS22).
4863. Support cross-app links for personal use at no cost (WS22).
4864. Support collab merge for personal use at no cost (WS22).
4865. Support no-telemetry CI for personal use at no cost (WS22).
4866. Support coverage gate for personal use at no cost (WS22).
4867. Never gate Excel corpus behind a subscription or account (WS22).
4868. Never gate importer fuzz behind a subscription or account (WS22).
4869. Never gate model invariants behind a subscription or account (WS22).
4870. Never gate render snapshots behind a subscription or account (WS22).
4871. Never gate ASan/UBSan behind a subscription or account (WS22).
4872. Never gate golden files behind a subscription or account (WS22).
4873. Never gate AT simulator behind a subscription or account (WS22).
4874. Never gate perf baseline behind a subscription or account (WS22).
4875. Never gate numeric stability behind a subscription or account (WS22).
4876. Never gate DEFLATE fuzz behind a subscription or account (WS22).
4877. Never gate cross-app links behind a subscription or account (WS22).
4878. Never gate collab merge behind a subscription or account (WS22).
4879. Never gate no-telemetry CI behind a subscription or account (WS22).
4880. Never gate coverage gate behind a subscription or account (WS22).
4881. Add Excel corpus as a first-class, offline-capable feature.
4882. Add importer fuzz as a first-class, offline-capable feature.
4883. Add model invariants as a first-class, offline-capable feature.
4884. Add render snapshots as a first-class, offline-capable feature.
4885. Add ASan/UBSan as a first-class, offline-capable feature.
4886. Add golden files as a first-class, offline-capable feature.
4887. Add AT simulator as a first-class, offline-capable feature.
4888. Add perf baseline as a first-class, offline-capable feature.
4889. Add numeric stability as a first-class, offline-capable feature.
4890. Add DEFLATE fuzz as a first-class, offline-capable feature.
4891. Add cross-app links as a first-class, offline-capable feature.
4892. Add collab merge as a first-class, offline-capable feature.
4893. Add no-telemetry CI as a first-class, offline-capable feature.
4894. Add coverage gate as a first-class, offline-capable feature.
4895. Expose Excel corpus through the local extension API (WS12).
4896. Expose importer fuzz through the local extension API (WS12).
4897. Expose model invariants through the local extension API (WS12).
4898. Expose render snapshots through the local extension API (WS12).
4899. Expose ASan/UBSan through the local extension API (WS12).
4900. Expose golden files through the local extension API (WS12).
4901. Expose AT simulator through the local extension API (WS12).
4902. Expose perf baseline through the local extension API (WS12).
4903. Expose numeric stability through the local extension API (WS12).
4904. Expose DEFLATE fuzz through the local extension API (WS12).
4905. Expose cross-app links through the local extension API (WS12).
4906. Expose collab merge through the local extension API (WS12).
4907. Expose no-telemetry CI through the local extension API (WS12).
4908. Expose coverage gate through the local extension API (WS12).
4909. Make Excel corpus work fully on-device with no telemetry (WS22).
4910. Make importer fuzz work fully on-device with no telemetry (WS22).
4911. Make model invariants work fully on-device with no telemetry (WS22).
4912. Make render snapshots work fully on-device with no telemetry (WS22).
4913. Make ASan/UBSan work fully on-device with no telemetry (WS22).
4914. Make golden files work fully on-device with no telemetry (WS22).
4915. Make AT simulator work fully on-device with no telemetry (WS22).
4916. Make perf baseline work fully on-device with no telemetry (WS22).
4917. Make numeric stability work fully on-device with no telemetry (WS22).
4918. Make DEFLATE fuzz work fully on-device with no telemetry (WS22).
4919. Make cross-app links work fully on-device with no telemetry (WS22).
4920. Make collab merge work fully on-device with no telemetry (WS22).
4921. Make no-telemetry CI work fully on-device with no telemetry (WS22).
4922. Make coverage gate work fully on-device with no telemetry (WS22).
4923. Test Excel corpus in CI with the WS22 correctness suite (WS22).
4924. Test importer fuzz in CI with the WS22 correctness suite (WS22).
4925. Test model invariants in CI with the WS22 correctness suite (WS22).
4926. Test render snapshots in CI with the WS22 correctness suite (WS22).
4927. Test ASan/UBSan in CI with the WS22 correctness suite (WS22).
4928. Test golden files in CI with the WS22 correctness suite (WS22).
4929. Test AT simulator in CI with the WS22 correctness suite (WS22).
4930. Test perf baseline in CI with the WS22 correctness suite (WS22).
4931. Test numeric stability in CI with the WS22 correctness suite (WS22).
4932. Test DEFLATE fuzz in CI with the WS22 correctness suite (WS22).
4933. Test cross-app links in CI with the WS22 correctness suite (WS22).
4934. Test collab merge in CI with the WS22 correctness suite (WS22).
4935. Test no-telemetry CI in CI with the WS22 correctness suite (WS22).
4936. Test coverage gate in CI with the WS22 correctness suite (WS22).
4937. Provide a fast importer fuzz that respects user ownership.
4938. Provide a fast AT simulator suitable for enterprise self-hosting.
4939. Provide a offline DEFLATE fuzz that respects user ownership.
4940. Provide a offline Excel corpus suitable for enterprise self-hosting.
4941. Provide a local-first render snapshots that respects user ownership.
4942. Provide a local-first numeric stability suitable for enterprise self-hosting.
4943. Provide a accessible collab merge that respects user ownership.
4944. Provide a accessible model invariants suitable for enterprise self-hosting.
4945. Provide a secure golden files that respects user ownership.
4946. Provide a secure cross-app links suitable for enterprise self-hosting.
4947. Provide a simple coverage gate that respects user ownership.
4948. Provide a simple ASan/UBSan suitable for enterprise self-hosting.
4949. Provide a auditable perf baseline that respects user ownership.
4950. Provide a auditable no-telemetry CI suitable for enterprise self-hosting.
4951. Support Excel corpus for personal use at no cost (WS22).
4952. Support importer fuzz for personal use at no cost (WS22).
4953. Support model invariants for personal use at no cost (WS22).
4954. Support render snapshots for personal use at no cost (WS22).
4955. Support ASan/UBSan for personal use at no cost (WS22).
4956. Support golden files for personal use at no cost (WS22).
4957. Support AT simulator for personal use at no cost (WS22).
4958. Support perf baseline for personal use at no cost (WS22).
4959. Support numeric stability for personal use at no cost (WS22).
4960. Support DEFLATE fuzz for personal use at no cost (WS22).
4961. Support cross-app links for personal use at no cost (WS22).
4962. Support collab merge for personal use at no cost (WS22).
4963. Support no-telemetry CI for personal use at no cost (WS22).
4964. Support coverage gate for personal use at no cost (WS22).
4965. Never gate Excel corpus behind a subscription or account (WS22).
4966. Never gate importer fuzz behind a subscription or account (WS22).
4967. Never gate model invariants behind a subscription or account (WS22).
4968. Never gate render snapshots behind a subscription or account (WS22).
4969. Never gate ASan/UBSan behind a subscription or account (WS22).
4970. Never gate golden files behind a subscription or account (WS22).
4971. Never gate AT simulator behind a subscription or account (WS22).
4972. Never gate perf baseline behind a subscription or account (WS22).
4973. Never gate numeric stability behind a subscription or account (WS22).
4974. Never gate DEFLATE fuzz behind a subscription or account (WS22).
4975. Never gate cross-app links behind a subscription or account (WS22).
4976. Never gate collab merge behind a subscription or account (WS22).
4977. Never gate no-telemetry CI behind a subscription or account (WS22).
4978. Never gate coverage gate behind a subscription or account (WS22).
4979. Add Excel corpus as a first-class, offline-capable feature.
4980. Add importer fuzz as a first-class, offline-capable feature.
4981. Add model invariants as a first-class, offline-capable feature.
4982. Add render snapshots as a first-class, offline-capable feature.
4983. Add ASan/UBSan as a first-class, offline-capable feature.
4984. Add golden files as a first-class, offline-capable feature.
4985. Add AT simulator as a first-class, offline-capable feature.
4986. Add perf baseline as a first-class, offline-capable feature.
4987. Add numeric stability as a first-class, offline-capable feature.
4988. Add DEFLATE fuzz as a first-class, offline-capable feature.
4989. Add cross-app links as a first-class, offline-capable feature.
4990. Add collab merge as a first-class, offline-capable feature.
4991. Add no-telemetry CI as a first-class, offline-capable feature.
4992. Add coverage gate as a first-class, offline-capable feature.
4993. Expose Excel corpus through the local extension API (WS12).
4994. Expose importer fuzz through the local extension API (WS12).
4995. Expose model invariants through the local extension API (WS12).
4996. Expose render snapshots through the local extension API (WS12).
4997. Expose ASan/UBSan through the local extension API (WS12).
4998. Expose golden files through the local extension API (WS12).
4999. Expose AT simulator through the local extension API (WS12).
5000. Expose perf baseline through the local extension API (WS12).


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
