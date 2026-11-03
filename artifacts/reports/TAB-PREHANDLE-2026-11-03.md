# Settings outer pre-handler — 2026-11-03

common/tab_prehandle.c now implements TabDialog's outer preHandleKeyPressMessage
body through owned backend callbacks, including focused/unfocused paths,
vertical/horizontal layout, circular navigation, edge jumps and close/repost
behavior. It remains separate from fallback handleMessage dispatch and the
unwired ui_vendor adapter. This is not a complete visible settings controller.

## Source and decisions

Shipped Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. The maintained instruction
and decompile source is analysis/tab-navigation-2026-11-03/keys-focus.txt,
entryd700a0. New analysis/tab-prehandle-2026-11-03/descriptor-return.txt and
verify-descriptor.cjs resolve literal d703c8 ->14d9097 to **vertical_tabs** and
confirm UIMessageTarget::handleKeyPressMessage114925c uses the pre-handler's
zero/nonzero return to decide whether to invoke fallback handleMessage.

The C API returns errno status separately from normalized handled0/1. We compare
native return truth, not raw values from noncanonical synthetic getters. This
preserves the tested caller's branch. No native memory-address return is a
portable controller ABI or evidence of a live vendor defect.

The method first reads description presence. Guide230/24c in media mode closes
all modal targets, posts a fresh key and requests/releases the image interface.
Keys25d/274 post a fresh key, unfocus/hide and end0. These precede the normal
group/content path. The image action is an actual interface request with source
operands0/1/1, not a harmless view-only getter; no such native request was run.

For ordinary keys a focused group target updates the local content-tab field
from its window ID, distinct from DemoApp retained state and group child index.
Unfocused content follows vertical-tabs/circular rules. Vertical Right selects
the smallest active item and requests focus, then **rechecks button activation**:
if false it delegates to the base pre-handler. The ARM branch d703bc ->d70526
and the differential caught this detail; do not infer an unconditional consumed
return from a flattened decompile. Vertical Up/Down may synthesize Left/Right
to the group, followed by title/show. Horizontal circular Up selects the biggest
item after show/focus; Down selects the smallest; base pre-handling still follows.

Focused content Left/Right receives the original event; the parent consumes it
even if the child reports unhandled. Menu22d/Quick251 forward then unfocus/hide/
end0; Menu additionally delegates to base. Exit210 asks shouldHandleBackKey,
otherwise activates tabs, selects the stored content-tab operand, clears child
focus and focuses the dialog. It does not silently remap that operand to a
created-child index.21f/28b end50 and push a fresh key to the queue front.

At Up/Down edges the method unfocuses, optionally invokes the recovered
Right/Left handler with a synthesized key, and applies vertical-layout selection/
focus behavior to the resulting content. Media2a7 unfocuses then delegates to
base. Other focused keys forward to content;25b additionally delegates to base.
Callback queries are pure current-state reads; actions can change state before
later queries. The backend must implement the previously recovered ownership,
permission and focus contracts; calling an action is not proof of its success.

CONTENT_KEY/BASE_PREHANDLE receive the exact original borrowed event. Synthesized
and posted keys have no borrowed message pointer and require a fresh owned event
from the adapter. No callback, code or message pointer is retained by this layer;
the caller must pin them and use nonblocking backend actions/owned GPU workers.

## Verification and deliberate bounds

The optional ARM/qemu reference executes the shipped1200-byte method unchanged
except its stack-guard pointer literal, CRC32 **0bec24b5**. Framework/string/key
construction and lower methods are controlled stubs. No vendor init/device or
TV process is involved; no vendor bytes are embedded/committed. Source hashes
are verified before private mappings, which refuse fixed overwrites.

**65,664 valid-tree cases** match exact action/argument/message traces, final
backend state and handled truth. Cases vary layout, content presence/focus,
media/group focus, circular/jump/back policies, edge position, post-jump presence,
independent button activation0/1/255, allowed focus, image availability and
child/base/group results0 or noncanonical nonzero.18 ICE keys cover the routing
classes and unknown/full-width inputs. All prior reference groups still pass.
The body is compared to the shipped function, not to another copy of our plan.

Invalid trees that would dereference a missing native child are excluded from
native execution. The new C API reports ENODATA at the required-child boundary,
possibly after earlier actions; it leaves handled unchanged. EINVAL for missing
callbacks/output executes none. Host tests cover those errors, including a show
that fails to supply content. This is explicit missing-state safety, not native
parity acceptance for those invalid states or proof the TV reaches them.

Strict host unit/full suite, focused ASan/UBSan and ARM/qemu policy checks pass.
Final isolated build-tv-v3/tab-policy pure test41588 bytes/MD5
a22c76e4c4d19719b0c646f7b74d2ae9; reference183692/
c7f77c6d0b832860186390ab7ca64262. Production module/installed ABI remain unchanged.
No TV access, wake, staging, deployment, keys, drawing or persistent mutation.

Next complete the fallback message map and wire owned renderer/content callbacks
and real runtime context into the queue consumer. Required defaults/cache/app/
permissions/native UI acceptance remain in TODO. Do not treat a green method
oracle as evidence of actual display execution or replace the captured-screen
shortcut with unconditional key dispatch.
