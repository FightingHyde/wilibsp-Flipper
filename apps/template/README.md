Starter app. Copy with `fw new-app <name>`. Links `freewili2_bsp`; include `fw2.h`.

For every published release, attach the validated `.uf2` as a downloadable
release artifact in the app repository. Do not require users to build it from
source or depend only on a short-lived CI artifact.

If this app lives in a public source repository, add an About screen containing
the app version and repository link. The usual unobtrusive gesture is holding
PAGE for five seconds; another discoverable menu or gesture is acceptable.
