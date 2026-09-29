# Protocol 21 medical capability

Protocol 21 is published atomically with canonical medical observation, native adjacent first-aid intent kind 10, authority preparation, and the passive controller. The projection and command bridge may be reviewed as separate local commits; neither intermediate capability is a release. The native command depends on reviewed native first aid (#907). The inventory activation/controller/panel dependencies remain required.

The session protocol is 21. A current client rejects a protocol-19 or protocol-20 server hello before sending credentials or campaign requests. The authority rejects old admission requests before actor assignment or campaign payload transfer. The listener may send its current hello and bootstrap descriptor before admission; those are compatibility metadata, not the campaign payload.

The existing tactical envelope remains version 4 and intent format remains version 5. Inventory swap remains kind 11. No campaign/save schema or command journal format changes here.

## Exact observation layout

Snapshot version 12 and delta version 11 use the same canonical medical bytes as the integrated reference. The released snapshot-9/delta-8 actor already has the full 44-byte presentation record. Snapshot versions 10 and 11 and delta versions 9 and 10 are reserved intermediate formats and rejected; no reader invents missing fields.

| Actor offset | Field | Encoding |
| --- | --- | --- |
| 136 | Bleeding | signed 16-bit little endian, 0–127 |
| 138 | Equipped hand kit points | unsigned 16-bit little endian |
| 140 | Providing service | canonical Boolean byte |
| 141 | Patient slot | unsigned 16-bit little endian |
| 143 | Patient incarnation | unsigned 32-bit little endian |
| 147 | Receiving provider count | unsigned byte |

Actor records are 148 bytes; full delta actor events are 149. Snapshot header stays 314 bytes and door records stay 7. Maximum public baseline payload is 45,370 bytes; maximum delta payload is 53,853, remaining below the existing 64 KiB transport ceiling.

An absent patient uses the exact invalid identity sentinel. Providing may be true while the patient is unresolved or withheld, and receiving is independent. Native capture names a patient only for a currently bound player teammate in the same sector and level; consumers resolve the full incarnation. Equipped kit points sum only the native counted stack, reject a short stack or any counted status above 100, and do not expose reserve pockets. Nonpositive depleted entries add zero. Capture is observational and does not start treatment.

## Old credentials

The fixed durable 224-byte records for protocols 19 and 20 are accepted only by the explicit retirement parser, alongside previously reviewed versions. Original checksum, semantic fields, campaign binding, and epoch rules remain required. Live decoders reject them, same-session mismatches cannot erase or replace them, and terminal retirement retains the original protocol/identity/token bytes. Private-copy normalization exists solely to apply strict semantic validation; it never relabels a stored or transmitted bearer.

## Native command bridge

Intent kind 10 appends only the exact patient slot (u16) and incarnation (u32) to the existing 72-byte intent header. Kind 11 inventory swap is unchanged; future kinds remain rejected. Malformed, truncated, and trailing input leaves the decoded output untouched. Admission authenticates and owns the medic, while the friendly patient may belong to the other peer. Public revision and turn binding remain exact.

The passive controller filters current friendly cardinal adjacency, medical need, equipped kit points, and service/readiness state. The native authority independently prepares the complete command proof, including exact identities, world/turn, grids, poses, complete kit fingerprint, and AP. Legacy networking cannot use this bridge. A queued command retains its outcome until the native result arrives; only normal authoritative publication updates inventory or health.

## Passive medical input

B opens adjacent first-aid selection for the currently owned medic with an equipped ordinary kit. Up/Down/Tab cycle eligible friendly patients; Enter sends one exact-patient intent, and Esc/Space cancels the selection. Other tactical, inventory and leave inputs are consumed while this mode owns the frame. Application close remains in the SDL application event pump. After treatment begins, the ordinary Space stop command cancels native service.

Selection retains exact medic/patient identity and authenticated owner/session/world/baseline/private-inventory revision. Fresh public views revalidate cardinal adjacency, medical need, service/readiness, and available matching ordinary idle pose. An unrelated public revision does not cancel selection: confirmation uses the current client revision. Identity, scope, readiness, kit or patient eligibility loss cancels without silently selecting someone else. Any private revision change conservatively revokes the retained kit choice because its complete hidden native metadata is not a client value.

The screen displays current bleeding, equipped kit points and service activity from the committed replica. It makes no item, health or AP prediction. The native authority remains responsible for hidden pending work, obstruction, medical skill, AP and startup proof. Queued native InputAtom tests exercise the shipping medical router with two real passive clients, their public replicas and private caches; complete Screen-loop, physical input and installed paint qualification remain separate.
