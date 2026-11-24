# Timeframe Builder

An application for merging multiple events from hepmc3 root files or simulated edm4hep files into timeframes. All collections of particles and hits are zipped together while maintaining the references between collections.

This tool provides control over timing adjustments allowing optional shifting of the time from each event source based on:
- Bunch crossing periods.
- Attachment of backgrounds to beam bunches.
- Additional Gaussian smearing.

Sources can either be individual events, or an already merged source allowing step by step overlays. *At the moment these seem to take about the same time but hopefully should be possible to speed up using already merged sources*

## Reporting
CI tests report on the comparison between the output of [eicrecon](https://github.com/eic/EICrecon) when hepmc3 events are merged before simulation with those merging edm4hep events after simulation. [Current capybara report from main](https://eic.github.io/TimeframeBuilder/capybara/main/index.html)

The memory usage profile during each step is also recorded and reported. [Current memory report from main](https://eic.github.io/TimeframeBuilder/memory/main/index.html)

## Prerequisites

- **PODIO library and headers** - Required for EDM4hep data I/O operations
- **EDM4HEP library and headers** - Required for the EDM4HEP data model
- **HepMC3 library and headers** - Optional, required for HepMC3 format support
- **yaml-cpp library** - Required for configuration file support
- **CMake 3.16 or later** - Required for building
- **C++20 compatible compiler** - Required for compilation

### Optional Dependencies

- **HepMC3**: If available, enables support for `.hepmc3.tree.root` format files. If not found during build, only EDM4hep format will be supported.

## Building

### Pre-commit Checks

The repository uses pre-commit hooks for YAML, whitespace, spelling, C++ formatting, and CMake formatting and linting. Install and enable them with:

```bash
python -m pip install pre-commit
pre-commit install
```

Run the checks across all tracked files with:

```bash
pre-commit run --all-files
```

### Quick Build

Use the provided build script for automatic configuration and compilation:

```bash
./build.sh
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
| `--config <file>` | YAML configuration file | (none) |
| `input_files` | Input ROOT files containing events | (required) |
| `-o, --output <file>` | Output ROOT file for timeframes | `merged_timeframes.root` |
| `-n, --nevents <number>` | Maximum number of timeframes to generate | `100` |

#### Timeframe Configuration
| Option | Description | Default |
|--------|-------------|---------|
| `-d, --duration <ns>` | Timeframe duration in nanoseconds | `20.0` |
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
| `--source:NAME:beam_speed SPEED` | Beam speed in ns/mm |
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
| `--beam-speed <ns/mm>` | Beam speed for time-of-flight calculations | `299792.458` |
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

### YAML Configuration Structure

```yaml
# Global merger configuration
output_file: merged_timeframes.root
max_events: 100
timeframe_duration: 200.0
bunch_crossing_period: 10.0
introduce_offsets: true

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
    beam_speed: 299792.458
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
- `introduce_offsets`: Whether to introduce random time offsets
- `merge_particles`: Whether to merge particles (advanced feature)

#### Source-Specific Parameters
- `input_files`: List of input ROOT files for this source
- `name`: Human-readable name for the source (e.g., "signal", "background")
- `already_merged`: Set to `true` if input files are already timeframe files
- `static_number_of_events`: Use fixed number of events per timeframe
- `static_events_per_timeframe`: Number of events per timeframe (if static mode)
- `mean_event_frequency`: Mean event frequency in events/ns (if not static mode)
- `use_bunch_crossing`: Enable bunch crossing time discretization
- `attach_to_beam`: Enable beam attachment physics
- `beam_angle`: Beam angle in radians for time-of-flight calculations
- `beam_speed`: Beam speed in ns/mm
- `beam_spread`: Gaussian time spread for beam smearing
- `generator_status_offset`: Offset to add to MCParticle generator status
- `tree_name`: Name of the input TTree (default: "events")

## Mixed Command Line and Configuration Usage

Command-line arguments override YAML configuration values, allowing flexible usage patterns:

### Pattern 1: Config File with CLI Override
```bash
# Use config.yml but override output file and number of events
./install/bin/timeframe_builder --config config.yml -o different_output.root -n 500
```

### Pattern 2: Config File with Additional Input Files
```bash
# Use config file and add more input files from command line
./install/bin/timeframe_builder --config config.yml additional_input.root more_files.root
```

### Pattern 3: Minimal Config with CLI Parameters
```bash
# Use config file for complex source setup, override timing parameters
./install/bin/timeframe_builder --config multi_source.yml -d 1000.0 -p 25.0
```

### Pattern 4: Source-Specific CLI Configuration
```bash
# Configure sources directly via command line
./install/bin/timeframe_builder --source:signal:input_files signal1.root,signal2.root --source:signal:frequency 0.5 --source:bg:input_files bg.root --source:bg:static_events true
```

### Pattern 5: Override Specific Sources from Config
```bash
# Use config file but override specific source configuration
./install/bin/timeframe_builder --config config.yml --source:signal:input_files new_signal.root --source:signal:frequency 0.8
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
  --beam-speed 299792.458 \
  -d 10000.0 \
  input_events.root
```

#### High Statistics Processing

Process large number of events with custom output:
```bash
./install/bin/timeframe_builder \
  -n 10000 \
  -o large_timeframes.root \
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
  --source:beam_events:beam_speed 299792.458 \
  --source:beam_events:beam_spread 0.5 \
  --source:beam_events:frequency 0.1
```

### YAML Configuration Usage

#### Using Configuration Files

Use a YAML configuration file for complex setups:
```bash
./install/bin/timeframe_builder --config config.yml
```

#### Using HepMC3 Files

Process HepMC3 format files (requires HepMC3 library):
```bash
# HepMC3 configuration file
./install/bin/timeframe_builder --config configs/config_hepmc3.yml

# Direct command line with HepMC3 files
./install/bin/timeframe_builder \
  -o output.hepmc3.tree.root \
  --source:signal:input_files signal.hepmc3.tree.root \
  --source:bg:input_files background.hepmc3.tree.root \
  --source:bg:static_events true \
  --source:bg:status_offset 1000
```

**Note**: The output format is automatically determined by the file extension:
- `.edm4hep.root` → EDM4hep format output
- `.hepmc3.tree.root` → HepMC3 format output
```bash
./install/bin/timeframe_builder --config config.yml
```

#### Mixed Configuration and Command Line

Use configuration file but override specific parameters:
```bash
# Override output file and number of timeframes
./install/bin/timeframe_builder --config config.yml -o custom_output.root -n 500

# Add additional input files to those specified in config
./install/bin/timeframe_builder --config config.yml extra_input.root

# Override timing parameters
./install/bin/timeframe_builder --config config.yml -d 5000.0 -p 500.0
```

#### Working with Pre-merged Timeframes

Process already-merged timeframe files:
```bash
# Configuration file with already_merged: true
./install/bin/timeframe_builder --config config_continue.yml
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

### EDM4hep Format (`.edm4hep.root`)
The output ROOT file contains:
- **Tree Name**: `timeframes`
- **Collections**: All input collections with time adjustments applied
- **Format**: Standard Podio ROOT format compatible with EDM4HEP readers
- **Additional Data**: Preserved metadata and collection relationships

### HepMC3 Format (`.hepmc3.tree.root`)
The output ROOT file contains:
- **Format**: HepMC3 ROOT tree format
- **Events**: Merged GenEvent objects with time offsets applied to vertices
- **Particles**: All particles with generator status offsets applied
- **Compatibility**: Compatible with HepMC3 readers and analysis tools

### Supported Collection Types

#### EDM4hep Format
The merger processes the following EDM4HEP collection types:
- **MCParticle**: Time and generator status updates
- **SimTrackerHit**: Time updates and particle reference mapping
- **SimCalorimeterHit**: Time updates and contribution processing
- **CaloHitContribution**: Time updates with proper particle references

#### HepMC3 Format
The merger processes HepMC3 GenEvent objects:
- **GenVertex**: Time offsets applied to vertex positions
- **GenParticle**: Generator status offsets applied to particles
- **Event Structure**: Preserved particle-vertex relationships

## Technical Implementation

### Architecture Overview

The application has been recently refactored from a Podio Frame-based approach to a more efficient vector-based ROOT processing system:

- **Direct ROOT I/O**: Uses ROOT TChain/TTree directly instead of Podio Frame readers
- **Vector-Based Processing**: Works with `std::vector<edm4hep::*Data>` types directly
- **Object-Oriented Design**: Clean separation between data sources and merged collections
- **Memory Efficient**: Streams data without loading entire files into memory

### Core Components

#### DataSource Class
Encapsulates input file management and data reading:
- Handles multiple input files through ROOT TChain
- Manages branch pointers for different collection types
- Provides event processing and time offset calculation methods

#### MergedCollections Structure
Organizes all merged output data:
- Event and particle collections (MCParticle, EventHeader)
- Hit collections (SimTrackerHit, SimCalorimeterHit, CaloHitContribution)
- Reference collections maintaining object relationships
- Global parameter (GP) branches for metadata

#### TimeframeBuilder
Main orchestration class:
- Initializes and manages multiple DataSource instances
- Coordinates timeframe generation and event merging
- Handles output file creation and metadata preservation

### Time Offset Generation
1. **Random Distribution**: Uniform random offsets within timeframe duration
2. **Bunch Crossing**: Optional discretization to bunch boundaries
3. **Beam Physics**: Time-of-flight corrections for realistic timing

### Particle Reference Mapping
- Maintains proper relationships between particles and detector hits
- Updates all cross-references when merging events
- Preserves parent-daughter relationships in particle hierarchies

### Memory Management
- Efficient streaming of large files without loading entire datasets
- Automatic cleanup of temporary objects
- Optimized for processing large-scale simulation outputs

## Error Handling

### Common Issues and Solutions

#### Missing Collections
- **Behavior**: Gracefully skips missing optional collections
- **Logging**: Reports which collections were not found
- **Impact**: Processing continues with available data

#### File Access Errors
- **Detection**: Automatic validation of input/output file accessibility
- **Reporting**: Clear error messages with file paths
- **Recovery**: Fails fast with descriptive error information

#### Invalid Parameters
- **Validation**: Command line arguments checked at startup
- **Examples**: Negative durations, invalid file paths
- **Response**: Helpful error messages with correct usage examples

#### Memory Issues
- **Large Files**: Automatic detection of memory constraints
- **Streaming**: Uses ROOT streaming for memory-efficient processing
- **Monitoring**: Use system tools like `htop` or `top` for resource monitoring

## Performance Considerations

### File Size Scaling
- **Input**: Linear scaling with number of input events
- **Output**: Depends on timeframe duration and event rate
- **Memory**: Constant memory usage regardless of file size

### Processing Speed
- **Typical Rate**: 1000-10000 events/second depending on complexity
- **Bottlenecks**: I/O operations and random number generation
- **Optimization**: Use SSDs and optimize bunch crossing parameters

## Troubleshooting

### Build Issues
1. **Missing Dependencies**: Ensure PODIO and EDM4HEP are installed and findable
2. **Compiler Errors**: Verify C++20 support in your compiler
3. **CMake Issues**: Check CMake version (3.16+ required)

### Runtime Issues
1. **File Not Found**: Verify input file paths and permissions
2. **Segmentation Faults**: Check input file format compatibility
3. **Memory Errors**: Reduce batch size or enable streaming mode

### Performance Issues
1. **Slow Processing**: Check disk I/O speeds and file system type
2. **Memory Usage**: Monitor with system tools (htop, top)
3. **Output Size**: Verify timeframe parameters are reasonable

## Development and Contributing

### Code Structure
- `src/TimeframeBuilder.cc`: Core merging logic and orchestration
- `src/DataSource.cc`: Input file management and data reading
- `src/timeframe_builder_main.cc`: Command line interface and configuration parsing
- `include/TimeframeBuilder.h`: Main API and data structures
- `include/DataSource.h`: Input data source abstraction
- `include/MergerConfig.h`: Configuration structures

### Testing
```bash
# Build the project
./build.sh

# Run basic configuration tests
./install/bin/test_standalone

# Test with sample data
./install/bin/timeframe_builder --config configs/config.yml
```

### Configuration Files
Example configuration files are provided in the `configs/` directory:
- `config.yml`: Basic multi-source configuration
- `config_continue.yml`: Working with pre-merged timeframe files

## License

This software is provided under the terms specified in the LICENSE file found in the top-level directory.

## Support

For questions, bug reports, or feature requests, please refer to the project's issue tracker or contact the development team.
