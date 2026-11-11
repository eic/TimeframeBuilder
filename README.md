# Timeframe Builder

An application for merging events from HepMC3 ROOT-tree files or Podio ROOT
files (including EDM4hep) into timeframes while preserving collection
associations.

This tool provides control over timing adjustments allowing optional shifting of the time from each event source based on:
- Bunch crossing periods.
- Attachment of backgrounds to beam bunches.
- Additional Gaussian smearing.

Sources can be individual events or already-merged frames, allowing
step-by-step overlays.

## Reporting
CI tests report on the comparison between the output of [eicrecon](https://github.com/eic/EICrecon) when hepmc3 events are merged before simulation with those merging edm4hep events after simulation. [Current capybara report from main](https://eic.github.io/TimeframeBuilder/capybara/main/index.html)

The memory usage profile during each step is also recorded and reported. [Current memory report from main](https://eic.github.io/TimeframeBuilder/memory/main/index.html)

## Prerequisites

- **PODIO library, compiled datamodel, and ROOT dictionary** - Optional,
  required for Podio ROOT I/O; EDM4HEP is selected by default when available
- **HepMC3 library and ROOT I/O component** - Optional, required for HepMC3
  format support
- **ROOT** - Required by either data-format backend
- **yaml-cpp library** - Required for configuration file support
- **Python 3 with PyYAML** - Required only for the Podio backend
- **CMake 3.16 or later** - Required for building
- **C++20 compatible compiler** - Required for compilation

The core scheduler and CLI can be built without either data-format backend.
CMake includes each backend's directory only when its dependencies are found.
Disable a backend explicitly with `-DTIMEFRAME_BUILDER_ENABLE_PODIO=OFF` or
`-DTIMEFRAME_BUILDER_ENABLE_HEPMC3=OFF`.

The top-level CMake file configures dependencies and selects backends;
[`core/CMakeLists.txt`](core/CMakeLists.txt),
[`podio/CMakeLists.txt`](podio/CMakeLists.txt), and
[`hepmc3/CMakeLists.txt`](hepmc3/CMakeLists.txt) configure their respective
source groups.

## Building

### Quick Build

Use the provided build script for automatic configuration, compilation, and
installation:

```bash
./make.sh
```

### Manual Build

For more control over the build process:

1. Create and enter build directory:
```bash
mkdir build && cd build
```

2. Configure with CMake:
```bash
cmake .. -DCMAKE_INSTALL_PREFIX=../install
```

3. Build and install:
```bash
make -j$(nproc)
make install
```

## Usage

### Basic Usage

```bash
./install/bin/timeframe_builder [options] input_file1 [input_file2 ...]
```

### Command Line Options

#### Configuration and File I/O Options
| Option | Description | Default |
|--------|-------------|---------|
| `--config <file>` | Per-run YAML configuration (sources and timing) | (none) |
| `--transform-config <file>` | Application/plugin transformation profile | (none; no transformations) |
| `input_files` | Input ROOT files containing events | (required) |
| `-o, --output <file>` | Output ROOT file for timeframes | `merged_timeframes.edm4hep.root` |
| `-n, --nevents <number>` | Maximum number of timeframes to generate | `100` |

#### Timeframe Configuration
| Option | Description | Default |
|--------|-------------|---------|
| `-d, --duration <ns>` | Timeframe duration in nanoseconds | `2000.0` |
| `-s, --static-events` | Use static number of events per timeframe (default source) | `false` |
| `-e, --events-per-frame <number>` | Static events per timeframe (requires `-s`) | `1` |
| `-f, --frequency <rate>` | Mean event frequency (events/ns) for default source | `1.0` |

#### Source-Specific Configuration
| Option | Description |
|--------|-------------|
| `--source:NAME` | Create or select a source named NAME |
| `--source:NAME:input_files FILE1,FILE2` | Comma-separated input files for the source |
| `--source:NAME:frequency RATE` | Mean event frequency (events/ns) |
| `--source:NAME:static_events BOOL` | Use static number of events (true/false) |
| `--source:NAME:events_per_frame N` | Static events per timeframe |
| `--source:NAME:bunch_crossing BOOL` | Enable bunch crossing logic (true/false) |
| `--source:NAME:beam_attachment BOOL` | Enable beam attachment (true/false) |
| `--source:NAME:beam_speed SPEED` | Beam speed in mm/ns |
| `--source:NAME:beam_spread SPREAD` | Gaussian beam time spread |
| `--source:NAME:status_offset OFFSET` | Generator status offset |

#### Bunch Crossing Options
| Option | Description | Default |
|--------|-------------|---------|
| `-b, --use-bunch-crossing` | Enable bunch crossing time discretization | `false` |
| `-p, --bunch-period <ns>` | Bunch crossing period in nanoseconds | `10.0` |

#### Beam Physics Options
| Option | Description | Default |
|--------|-------------|---------|
| `--beam-attachment` | Enable beam attachment for particles | `false` |
| `--beam-speed <mm/ns>` | Beam speed for time-of-flight calculations | `299.792458` |
| `--beam-spread <ns>` | Gaussian beam time spread (std deviation) | `0.0` |

#### Generator Configuration
| Option | Description | Default |
|--------|-------------|---------|
| `--status-offset <value>` | Offset added to MCParticle generator status | `0` |

#### Utility Options
| Option | Description |
|--------|-------------|
| `-h, --help` | Display help message and exit |

## Configuration Files

The application supports YAML configuration files that can be used independently or in combination with command-line arguments.
Run configurations such as [config.yml](configs/config.yml) and
[config_hepmc3.yml](configs/config_hepmc3.yml) hold per-run inputs and timing.
Transformation bindings are separate application profiles:
[Podio/EDM4hep](configs/timeframe_builder/podio_transformations.yml) and
[HepMC3](configs/timeframe_builder/hepmc3_transformations.yml).

### YAML Configuration Structure

```yaml
# Global merger configuration
output_file: merged_timeframes.edm4hep.root
max_events: 100
timeframe_duration: 200.0
bunch_crossing_period: 10.0
# Source-specific configuration
sources:
  - input_files:
      - input1.root
      - input2.root
    name: signal
    static_number_of_events: false
    mean_event_frequency: 0.1
    use_bunch_crossing: true
    attach_to_beam: false
    beam_angle: 0.0
    beam_speed: 299.792458
    beam_spread: 0.0
    generator_status_offset: 0
    already_merged: false
    tree_name: events
  - input_files:
      - background.root
    name: background
    static_number_of_events: true
    static_events_per_timeframe: 2
    use_bunch_crossing: true
    generator_status_offset: 1000
    already_merged: false
```

### YAML Configuration Parameters

#### Global Parameters
- `output_file`: Output ROOT file name
- `max_events`: Maximum number of timeframes to generate
- `timeframe_duration`: Duration of each timeframe in nanoseconds
- `bunch_crossing_period`: Bunch crossing period for discretization

### Application Transformation Profiles

Pass a separate profile with `--transform-config`; transformation rules are
not part of run configurations and are not enabled implicitly. The timeframe
builder wrapper parses the profile and passes its event and record rules to
`EventMerger`.

The [Podio profile](configs/timeframe_builder/podio_transformations.yml)
applies `time_offset` to numeric `time` members and applies each source's
`generator_status_offset` to `edm4hep::MCParticle.generatorStatus` in
`MCParticles`. The [HepMC3 profile](configs/timeframe_builder/hepmc3_transformations.yml)
targets `HepMC3::GenVertex / vertices / position.t` and
`HepMC3::GenParticle / particles / status`. Each record rule specifies a
plugin, datatype, collection, field, and plugin parameters.

`source_time_offset` is an event-level plugin that computes the event context
from run/source timing and beam settings. `add_context_value` applies a context
value to the selected numeric field; `add_field_value` derives a field from
another numeric field in the same record. Omit the profile or either rule
section to leave those transformations unapplied.

#### Source-Specific Parameters
- `input_files`: List of input ROOT files for this source
- `name`: Human-readable name for the source (e.g., "signal", "background")
- `already_merged`: Set to `true` if input files are already timeframe files
- `static_number_of_events`: Use fixed number of events per timeframe
- `static_events_per_timeframe`: Number of events per timeframe (if static mode)
- `mean_event_frequency`: Mean event frequency in events/ns (if not static mode)
- `use_bunch_crossing`: Enable bunch crossing time discretization
- `attach_to_beam`: Enable beam attachment in the configured
  `source_time_offset` event plugin
- `beam_angle`: Beam angle in radians for time-of-flight calculations
- `beam_speed`: Beam speed in mm/ns
- `beam_spread`: Gaussian time spread for beam smearing
- `generator_status_offset`: Value exposed to record plugins as the
  `generator_status_offset` context value
- `tree_name`: Input Podio category or HepMC3 tree name (default: "events")

## Mixed Command Line and Configuration Usage

Command-line arguments override YAML configuration values, allowing flexible usage patterns:

### Pattern 1: Config File with CLI Override
```bash
# Use config.yml but override output file and number of events
./install/bin/timeframe_builder --config configs/config.yml --transform-config configs/timeframe_builder/podio_transformations.yml -o different_output.root -n 500
```

### Pattern 2: Config File with Additional Input Files
```bash
# Use config file and add more input files from command line
./install/bin/timeframe_builder --config configs/config.yml --transform-config configs/timeframe_builder/podio_transformations.yml additional_input.root more_files.root
```

### Pattern 3: Minimal Config with CLI Parameters
```bash
# Use config file for complex source setup, override timing parameters
./install/bin/timeframe_builder --config multi_source.yml --transform-config configs/timeframe_builder/podio_transformations.yml -d 1000.0 -p 25.0
```

### Pattern 4: Source-Specific CLI Configuration
```bash
# Configure sources directly via command line
./install/bin/timeframe_builder --source:signal:input_files signal1.root,signal2.root --source:signal:frequency 0.5 --source:bg:input_files bg.root --source:bg:static_events true
```

### Pattern 5: Override Specific Sources from Config
```bash
# Use config file but override specific source configuration
./install/bin/timeframe_builder --config configs/config.yml --transform-config configs/timeframe_builder/podio_transformations.yml --source:signal:input_files new_signal.root --source:signal:frequency 0.8
```


## Usage Examples

### Basic Command Line Usage

#### Basic Event Merging

Merge events using default settings:
```bash
./install/bin/timeframe_builder input_events.root
```

#### Static Event Count

Create timeframes with exactly 3 events each:
```bash
./install/bin/timeframe_builder -s -e 3 -n 50 input_events.root
```

#### Bunch Crossing Mode

Enable bunch crossing with 1000 ns period:
```bash
./install/bin/timeframe_builder -b -p 1000.0 -d 20000.0 input_events.root
```

#### Full Beam Physics

Enable beam attachment with realistic parameters:
```bash
./install/bin/timeframe_builder \
  --beam-attachment \
  --beam-spread 0.5 \
  --beam-speed 299.792458 \
  -d 10000.0 \
  input_events.root
```

#### High Statistics Processing

Process large number of events with custom output:
```bash
./install/bin/timeframe_builder \
  -n 10000 \
  -o large_timeframes.edm4hep.root \
  --use-bunch-crossing \
  --bunch-period 2000.0 \
  input1.root input2.root input3.root
```

### Source-Specific CLI Usage

#### Creating Multiple Sources via CLI

Configure multiple sources with different properties:
```bash
# Create signal and background sources with specific settings
./install/bin/timeframe_builder \
  --source:signal:input_files signal1.root,signal2.root \
  --source:signal:frequency 0.5 \
  --source:signal:static_events false \
  --source:background:input_files bg1.root,bg2.root \
  --source:background:static_events true \
  --source:background:events_per_frame 2 \
  --source:background:status_offset 1000
```

#### Simple Two-Source Setup

Quick setup with signal and background:
```bash
./install/bin/timeframe_builder \
  --source:signal:input_files signal.root \
  --source:bg:input_files background.root \
  --source:bg:static_events true
```

#### Advanced Beam Physics Configuration

Configure source with beam physics:
```bash
./install/bin/timeframe_builder \
  --source:beam_events:input_files beam_data.root \
  --source:beam_events:beam_attachment true \
  --source:beam_events:beam_speed 299.792458 \
  --source:beam_events:beam_spread 0.5 \
  --source:beam_events:frequency 0.1
```

### YAML Configuration Usage

#### Using Configuration Files

Use a YAML configuration file for complex setups:
```bash
./install/bin/timeframe_builder --config configs/config.yml --transform-config configs/timeframe_builder/podio_transformations.yml
```

#### Using HepMC3 Files

Process HepMC3 format files (requires HepMC3 library):
```bash
# HepMC3 configuration file
./install/bin/timeframe_builder --config configs/config_hepmc3.yml --transform-config configs/timeframe_builder/hepmc3_transformations.yml

# Direct command line with HepMC3 files
./install/bin/timeframe_builder \
  -o output.hepmc3.tree.root \
  --source:signal:input_files signal.hepmc3.tree.root \
  --source:bg:input_files background.hepmc3.tree.root \
  --source:bg:static_events true \
  --source:bg:status_offset 1000
```

**Note**: The output format is automatically determined by the file extension:
- `.edm4hep.root` and `.podio.root` → buffer-oriented podio handler
- `.hepmc3.tree.root` → HepMC3 format output

#### Mixed Configuration and Command Line

Use configuration file but override specific parameters:
```bash
# Override output file and number of timeframes
./install/bin/timeframe_builder --config configs/config.yml --transform-config configs/timeframe_builder/podio_transformations.yml -o custom_output.edm4hep.root -n 500

# Add additional input files to those specified in config
./install/bin/timeframe_builder --config configs/config.yml --transform-config configs/timeframe_builder/podio_transformations.yml extra_input.root

# Override timing parameters
./install/bin/timeframe_builder --config configs/config.yml --transform-config configs/timeframe_builder/podio_transformations.yml -d 5000.0 -p 500.0
```

#### Working with Pre-merged Timeframes

Process already-merged timeframe files:
```bash
# Configuration file with already_merged: true
./install/bin/timeframe_builder --config configs/config_continue.yml --transform-config configs/timeframe_builder/podio_transformations.yml
```

## Configuration Parameters Details

### Timeframe Duration (`-d, --duration`)
- **Units**: nanoseconds
- **Purpose**: Sets the total time span of each output timeframe
- **Usage**: Larger values create longer timeframes with more temporal coverage
- **Typical Values**: 10000-50000 ns for physics applications

### Event Accumulation Modes

#### Static Mode (`-s, --static-events`)
- Uses a fixed number of events per timeframe (set by `-e`)
- Guarantees consistent event count across all timeframes
- Useful for systematic studies and detector calibration

#### Poisson Mode (default)
- Uses Poisson distribution based on mean frequency (`-f`)
- More realistic for simulating random event timing
- Better for physics analysis with realistic trigger rates

### Bunch Crossing Logic (`-b, --use-bunch-crossing`)
- **Purpose**: Discretizes event timing to bunch crossing boundaries
- **Implementation**: Uses `floor(time / bunch_period) * bunch_period`
- **Physics**: Simulates accelerator bunch structure
- **Use Case**: Required for realistic collider simulations

### Beam Attachment (`--beam-attachment`)
- **Purpose**: Adjusts particle timing based on time-of-flight from interaction vertex
- **Physics**: Accounts for finite beam propagation speed
- **Implementation**: Adds Gaussian-smeared time-of-flight corrections
- **Required For**: Realistic detector timing simulations

## Output Format

The application supports multiple output formats based on the file extension:

### Podio Format (`.podio.root`, `.edm4hep.root`)
The buffer-oriented podio handler reads ROOTFrameData without constructing a
Frame for each input event, merges collection buffers, and writes native podio
ROOT output. Typed collection adapters and numeric field adapters are generated
at build time from the datamodel YAML files selected for the build. Generation
handles scalar numeric members, one-to-one and one-to-many relations, and
vector members. The generated adapters are build outputs, not model-specific
source files in the main codebase.

EDM4hep's installed schema is selected automatically when available. To build
against additional local datamodels, pass their YAML schemas at configure time
and link their generated libraries in CMake:

```bash
cmake -S . -B build \
  -DTIMEFRAME_BUILDER_DATAMODEL_YAMLS="/path/to/edm4hep.yaml;/path/to/custom.yaml" \
  -DTIMEFRAME_BUILDER_DATAMODEL_PACKAGES=CustomModel \
  -DTIMEFRAME_BUILDER_DATAMODEL_LIBRARIES="CustomModel::core;CustomModel::dict"
```

The list must include the schemas corresponding to datamodels used by the
merger; linking a model library alone does not expose its schema to CMake.
Use the package and target names exported by the local datamodel installation.

Beam attachment uses the configured `beam_vertex_collection` (default
`MCParticles`) and a generated accessor for a datatype with numeric
`generatorStatus` plus an x/y/z `vertex` component. When input frames contain
an EventHeader-like `EventHeader` collection, the handler emits one timeframe
`EventHeader` and per-source `SubEventHeaders`, matching the legacy output
convention.
Frame-level GenericParameters are retained from the first contributing input
event in each timeframe.

Both extensions now write native podio ROOT frames. Legacy custom ROOT trees
that lack podio frame metadata are not supported by this handler.

### HepMC3 Format (`.hepmc3.tree.root`)
The output ROOT file contains:
- **Format**: HepMC3 ROOT tree format
- **Events**: Merged GenEvent objects with time offsets applied to vertices
- **Particles**: All particles with generator status offsets applied
- **Compatibility**: Compatible with HepMC3 readers and analysis tools

#### HepMC3 Format
The merger processes HepMC3 GenEvent objects:
- **GenVertex**: Time offsets applied to vertex positions
- **GenParticle**: Generator status offsets applied to particles
- **Event Structure**: Preserved particle-vertex relationships

## Architecture

[`EventMerger`](core/src/EventMerger.cc) schedules events according to the
supplied configuration. The abstract [`DataHandler`](core/include/DataHandler.h) owns the
format-specific input and output workflow:

- [`PodioDataHandler`](podio/include/PodioDataHandler.h) reads and writes native
  Podio ROOT frames using collection buffers. Its typed adapters are generated
  at build time from the selected datamodel YAML schemas.
- [`HepMC3DataHandler`](hepmc3/include/HepMC3DataHandler.h) handles HepMC3 ROOT-tree
  input and output when HepMC3 is available at build time.
- [`DataSource`](core/include/DataSource.h) provides shared source state and timing
  behavior; concrete sources implement format-specific input.

The output filename selects the handler. Inputs used in a run must be
compatible with that handler.

## Testing

Build the project and run its registered tests:

```bash
./make.sh
ctest --test-dir build --output-on-failure
```

The CTest suite covers event-plugin source selection, record field transforms,
and numeric range checks. It does not yet cover full Podio buffer merging or
HepMC3 file I/O.

### Configuration Files
Example configuration files are provided in the `configs/` directory:
- `config.yml`: Basic multi-source configuration
- `config_continue.yml`: Working with pre-merged timeframe files

## Support

For questions, bug reports, or feature requests, please refer to the project's issue tracker or contact the development team.
