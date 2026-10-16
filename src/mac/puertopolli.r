/* Risorse Macintosh di Puerto Polli */
#include "Processes.r"
#include "MacTypes.r"

resource 'SIZE' (-1) {
	reserved,
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	doesActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	notHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	1024 * 1024,
	640 * 1024
};

resource 'vers' (1) {
	0x01, 0x00, release, 0x00,
	4,		/* verItaly */
	"1.0",
	"1.0, Puerto Polli - un'avventura a Puerto Polli"
};

#include "Dialogs.r"

/* finestra "Informazioni su Puerto Polli..." */
resource 'ALRT' (128) {
	{ 80, 76, 210, 436 },
	128,
	{
		OK, visible, silent;
		OK, visible, silent;
		OK, visible, silent;
		OK, visible, silent
	},
	noAutoCenter
};

resource 'DITL' (128) {
	{
		{ 96, 270, 116, 340 },
		Button { enabled, "OK" };

		{ 12, 16, 30, 340 },
		StaticText { disabled, "Puerto Polli 1.0" };

		{ 36, 16, 88, 340 },
		StaticText { disabled, "Un'avventura grafica in stile Monkey Island "
		                       "per Macintosh Classic. Mai fidarsi di un pollo." }
	}
};
