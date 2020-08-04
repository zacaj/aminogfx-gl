'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play H265 (HEVC) video.
 *
 * https://x265.com/hevc-video-files/
 */

player.playVideo({
    //1920x800, 24 fps
    src: path.join(__dirname, 'Tears_400_x265.mp4'),
    opts: 'amino_dump_format=1'
}, (_err, _video) => {
    //empty
});