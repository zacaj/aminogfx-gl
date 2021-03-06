'use strict';

const path = require('path');
const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

//fonts
amino.fonts.registerFont({
    name: 'Oswald',
    path: path.join(__dirname, 'resources/oswald/'),
    weights: {
        200: {
            normal: 'Oswald-Light.ttf'
        },
        400: {
            normal: 'Oswald-Regular.ttf'
        },
        800: {
            normal: 'Oswald-Bold.ttf'
        }
    }
});

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //root
    const root = this.createGroup();

    this.w(512);
    this.h(512);
    this.fill('#FFFFFF');
    //this.fill('#000000');

    this.setRoot(root);

    //image
    const iv = this.createImageView();

    root.add(iv);

    //text
    const text = this.createText().fontName('Oswald')
        .text('The quick brown fox jumps over the lazy dog.')
        .y(200) //outside viewport
        .w(512)
        .fill('#0000FF')
        .vAlign('top')
        .wrap('word')
        .fontSize(80)
        .fontWeight(200);

    text.font.watch((font) => {
        const texture = this.createTexture();

        texture.loadTexture(font, err => {
            if (err) {
                console.log('could not load texture: ' + err.message);
                return;
            }

            console.log('texture: ' + texture.w + 'x' + texture.h);

            iv.image(texture);
        });
    });

    root.add(text); //add to render glyphs
});
