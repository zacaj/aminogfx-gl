'use strict';

const amino = require('../../main.js');

//create instance
const gfx = new amino.AminoGfx({
    resolution: '1080p@60',

    //multi-display support
    display: 'HDMI-A-2' //Pi 4: HDMI 2
});

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
});
