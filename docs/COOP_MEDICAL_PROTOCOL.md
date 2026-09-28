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
