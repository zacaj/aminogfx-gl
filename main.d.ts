declare module "aminogfx-gl" {
    export class AminoGfx {
        start(done: (err?: any) => void): void;

        w: Property<this>;
        h: Property<this>;
        title: Property<this, string>;
        fill: Property<this, string>;
        showFPS: Property<this, boolean>;

        screen: {
            w: number;
            h: number;
            fullscreen: boolean;
        }

        setRoot(root: Group): void;
        createGroup(): Group;
        createRect(): Rect;
        createImageView(): ImageView;
        createCircle(): Circle;
        createText(): Text;
        createTexture(): Texture;
        on(type: 'press', node: Node|null, cb: (e: {
            point: { x: number, y: number },
            type: 'press',
            target: Node,
            button: number,
        }) => void): void;
        on(type: 'key.press', node: Node|null, cb: (e: {
            type: 'key.press',
            keycode: number,
            char?: string,
            shift?: boolean,
        }) => void): void;

        inputHandler: {
            statusObjects: {
                keyboard: {
                    state: { [keycode: number]: boolean };
                };
            };
        };
    }
    export module fonts {
        export function registerFont(font: Font): void;
    }
    export type Font = {
        name: string; // referenced by Text.fontName
        path: string; // folder containing font file names
        weights: { [weight: number]: {
            normal: string; // file name
        }};
    };

    export type Property<O extends {}, T = number> =
        ((value: T) => O) & (() => T)
    & {
        readonly: boolean;
        anim(props?: {
            from?: number;
            to?: number;
            dur?: number;
            delay?: number;
            loop?: number;
            then?: () => void;
            autoreverse?: boolean;
            timefunc?: 'linear'|'cubicIn'|'cubicOut'|'cubicInOut';
        }): Anim;
        watch(prop: Property<any, T>): O;
    }

    export abstract class Node {
        x: Property<this>;
        y: Property<this>;
        z: Property<this>;
        w: Property<this>;
        h: Property<this>;
        originX: Property<this>;
        originY: Property<this>;
        sx: Property<this>;
        sy: Property<this>;
        rx: Property<this>;
        ry: Property<this>;
        rz: Property<this>;
        visible: Property<this, boolean>;
        opacity: Property<this>;
        id: Property<this, string>;

        acceptsMouseEvents: boolean;
        acceptsKeyboardEvents: boolean;

        constructor(gfx: AminoGfx);
    }

    export class Group extends Node {
        isGroup: true;
        children: Node[];

        add(...nodes: Node[]): this;
        remove(...nodes: Node[]): this;
        clear(): this;
    }

    export class Rect extends Node {
        r: Property<this>;
        g: Property<this>;
        b: Property<this>;
        fill: Property<this, string>;
    }

    export class ImageView extends Node {
        left: Property<this>;
        right: Property<this>;
        top: Property<this>;
        bottom: Property<this>;
        size: Property<this, 'resize'|'contain'|'stretch'>;
        src: Property<this, string|AminoImage>;
        image: Property<this, Texture>;
    }
    export class Texture {
        loadTextureFromImage(img: AminoImage, cb: (err?: Error) => void): void;
    }

    export class Polygon extends Node {
        fill: Property<this, string>;
        filled: Property<this, boolean>;
    }

    export class Circle extends Polygon {
        radius: Property<this>;
    }

    export class AminoImage {
        src: string;
        onload?: (err?: any) => void;
    }

    export class Text extends Node {
        text: Property<this, string>;
        fontSize: Property<this, number>;
        fontName: Property<this, string>;
        fontWeight: Property<this, number>;
        fontStyle: Property<this, 'normal'>;

        //color
        r: Property<this>;
        g: Property<this>;
        b: Property<this>;
        fill: Property<this, string>;

        //alignment
        align:  Property<this, 'left'|'center'|'right'>;
        vAlign: Property<this, 'baseline'|'top'|'middle'|'bottom'>;
        wrap:   Property<this, 'none'|'word'|'end'>;

        //lines
        maxLines: Property<this, number>;
    }

    export class Anim {
        from(val: number): this;
        to(val: number): this;
        dur(ms: number): this;
        delay(ms: number): this;
        loop(times: number): this; // -1 for infinite
        then(cb: () => void): this;
        autoreverse(val: boolean): this;
        timefunc(func: 'linear'|'cubicIn'|'cubicOut'|'cubicInOut'): this;
        start(refTime?: number): this;
    }
}