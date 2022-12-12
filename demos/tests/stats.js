'use strict';

const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    this.fill('#FF0000');

    //some info
    console.log('screen: ' + JSON.stringify(gfx.screen));
    console.log('window size: ' + this.w() + 'x' + this.h());
    console.log('runtime: ' + JSON.stringify(gfx.runtime));
    console.log('time: ' + this.getTime());

    setInterval(() => {
        const stats = gfx.getStats();

        console.log('stats: ' + JSON.stringify(stats));

        //monitor
        showMonitorState(gfx.getMonitor());
    }, 1000);
});

//HDMI states (Pi 3)
const VC_HDMI_UNPLUGGED          = (1 << 0);  // HDMI cable is detached
const VC_HDMI_ATTACHED           = (1 << 1);  // HDMI cable is attached but not powered on
const VC_HDMI_DVI                = (1 << 2);  // HDMI is on but in DVI mode (no audio)
const VC_HDMI_HDMI               = (1 << 3);  // HDMI is on and HDMI mode is active
//const VC_HDMI_HDCP_UNAUTH        = (1 << 4);  // HDCP authentication is broken (e.g. Ri mismatched) or not active
//const VC_HDMI_HDCP_AUTH          = (1 << 5);  // HDCP is active
//const VC_HDMI_HDCP_KEY_DOWNLOAD  = (1 << 6);  // HDCP key download successful/fail
//const VC_HDMI_HDCP_SRM_DOWNLOAD  = (1 << 7);  // HDCP revocation list download successful/fail
const VC_HDMI_CHANGING_MODE      = (1 << 8);  // HDMI is starting to change mode, clock has not yet been set

/**
 * Display current HDMI state.
 *
 * @param {*} stats
 */
function showMonitorState(monitor) {
    //DRM version (Pi 4)
    const connected = monitor.connected;

    if (connected !== undefined) {
        if (connected) {
            console.log('HDMI: connected');
        } else {
            console.log('HDMI: disconnected');
        }
    }

    //legacy (Pi 3)
    const state = monitor.state;

    if (state !== undefined) {
        if (state & VC_HDMI_UNPLUGGED) {
            console.log('HDMI: unplugged');
        } else if (state & VC_HDMI_ATTACHED) {
            console.log('HDMI: attached');
        } else if (state & VC_HDMI_DVI) {
            console.log('HDMI: on (DVI mode)');
        } else if (state & VC_HDMI_HDMI) {
            console.log('HDMI: on');
        } else if (state & VC_HDMI_CHANGING_MODE) {
            console.log('HDMI: changing mode');
        } else {
            console.log('HDMI: ???');
        }
    }

    //EDID
    const edid = monitor.edid;

    if (edid) {
        console.dir(edid);
    }
}