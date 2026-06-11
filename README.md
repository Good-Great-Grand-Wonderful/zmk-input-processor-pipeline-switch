# ZMK Pipeline Switch Input Processor

A meta input processor for ZMK that routes events through one of several
pipelines of existing input processors, switchable at runtime with a
keypress. The active pipeline can be persisted to flash and restored at
power on. Use it to flip a trackball between e.g. cursor mode and scroll
mode without dedicating a layer to it.

Includes two devicetree compatibles:

- `zmk,input-processor-pipeline-switch` — the processor. Its child nodes are
  the pipelines (indexed 0..N-1 in declaration order), each with its own
  `input-processors` list of ordinary processors.
- `zmk,behavior-pipeline-switch` — the toggle behavior: each press advances
  to the next pipeline, wrapping around.

Pipeline iteration, per-entry parameters, and remainder tracking mirror
ZMK's own input listener (`app/src/pointing/input_listener.c`), so wrapped
processors behave exactly as they would directly under a listener.

## Installation

Add to your `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: gggw
      url-base: https://github.com/Good-Great-Grand-Wonderful
  projects:
    - name: zmk-input-processor-pipeline-switch
      remote: gggw
      revision: main
```

## Usage

```dts
#include <input/processors.dtsi>

/ {
    zip_ball_mode: zip_ball_mode {
        compatible = "zmk,input-processor-pipeline-switch";
        #input-processor-cells = <0>;
        persistent;

        cursor { // index 0
            input-processors = <&zip_xy_transform (INPUT_TRANSFORM_X_INVERT | INPUT_TRANSFORM_Y_INVERT)>;
        };

        scroll { // index 1
            input-processors
              = <&zip_xy_to_scroll_mapper>
              , <&zip_scroll_scaler 1 4>;
        };
    };

    behaviors {
        ball_mode: ball_mode {
            compatible = "zmk,behavior-pipeline-switch";
            #binding-cells = <0>;
            processor = <&zip_ball_mode>;
            display-name = "Ball Mode";
        };
    };
};

&trackball_listener {
    input-processors = <&zip_ball_mode>;
};
```

Then bind `&ball_mode` in your keymap. Other processors can sit alongside
the switch in the listener's list (before or after it).

### Processor properties

| Property     | Type    | Required | Description                                            |
| ------------ | ------- | -------- | ------------------------------------------------------ |
| `persistent` | boolean | no       | Persist the active index to flash (requires settings).  |
| `save_delay` | int     | no       | Debounce in ms before saving to flash (default 2000).   |

Child nodes: each child is one pipeline and requires an `input-processors`
phandle-array. At least one child is required (enforced at compile time).

### Behavior properties

| Property    | Type    | Required | Description                            |
| ----------- | ------- | -------- | -------------------------------------- |
| `processor` | phandle | yes      | The pipeline-switch node to control.   |

## Split keyboards

The behavior uses `BEHAVIOR_LOCALITY_EVENT_SOURCE`: it runs on whichever
half the key is physically pressed on, and switches that half's processor
instance. Place the binding on the half whose listener (or input-split
device) runs the pipelines — for a central-side listener that means a key on
the central half. Behavior **node names** (labels don't matter) must be 8
characters or fewer: the BLE split relay truncates behavior names to
`ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN` (9 bytes including the terminator) and the
peripheral resolves the behavior by the truncated name, which then fails.

## How a switch behaves

- The new pipeline takes effect on the next input event; fractional
  remainders of the incoming pipeline are reset on switch.
- A wrapped processor returning `ZMK_INPUT_PROC_STOP` stops the listener's
  whole chain, exactly as it would unwrapped.
- Sub-processors keep their own internal state; the switch only selects
  which chain events flow through.

## Persistence

`persistent` requires the Zephyr settings subsystem (a storage partition and
`CONFIG_SETTINGS`, which ZMK BLE/Studio setups typically already have).
Saves are debounced by `save_delay`. On a split, each half stores its own
selection.
