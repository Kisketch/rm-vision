import re
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime, timedelta
import matplotlib
import numpy as np
from matplotlib.patches import Rectangle

# 设置字体
matplotlib.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial', 'sans-serif']
matplotlib.rcParams['axes.unicode_minus'] = False

def parse_log_file(file_path):
    """Parse log file, extract timestamps and parameter values"""
    data = {
        'time': [],  # timestamps
        'R_ypd_y': [], 'R_ypd_p': [], 'R_ypd_d': [],  # Input - PowerRune Data
        'ypr_y': [], 'ypr_p': [], 'ypr_r': [],       # ypr
        'Blade_ypd_y': [], 'Blade_ypd_p': [], 'Blade_ypd_d': [],  # Blade_ypd
        'R_yaw': [], 'v_R_yaw': [], 'R_pitch': [], 'R_dis': [],  # State After Update
        'yaw': [], 'roll': [], 'spd': []
    }
    
    # Define regex patterns for key lines
    time_pattern = re.compile(r'\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\]')
    input_pattern = re.compile(r'R_ypd\[([-\d\.]+), ([-\d\.]+), ([-\d\.]+)\], ypr\[([-\d\.]+), ([-\d\.]+), ([-\d\.]+)\], Blade_ypd\[([-\d\.]+), ([-\d\.]+), ([-\d\.]+)\]')
    state_pattern = re.compile(r'R_yaw: ([-\d\.]+), v_R_yaw: ([-\d\.]+), R_pitch: ([-\d\.]+), R_dis: ([-\d\.]+), yaw: ([-\d\.]+), roll: ([-\d\.]+), spd: ([-\d\.]+)')
    
    with open(file_path, 'r', encoding='utf-8') as f:
        current_time = None
        for line in f:
            # 1. Extract timestamp
            time_match = time_pattern.search(line)
            if time_match:
                try:
                    current_time = datetime.strptime(time_match.group(1), '%Y-%m-%d %H:%M:%S.%f')
                except ValueError:
                    continue
            
            # 2. Parse Input - PowerRune Data line
            input_match = input_pattern.search(line)
            if input_match and current_time:
                r_y, r_p, r_d, y, p, r, b_y, b_p, b_d = map(float, input_match.groups())
                data['time'].append(current_time)
                data['R_ypd_y'].append(r_y)
                data['R_ypd_p'].append(r_p)
                data['R_ypd_d'].append(r_d)
                data['ypr_y'].append(y)
                data['ypr_p'].append(p)
                data['ypr_r'].append(r)
                data['Blade_ypd_y'].append(b_y)
                data['Blade_ypd_p'].append(b_p)
                data['Blade_ypd_d'].append(b_d)
                data['R_yaw'].append(None)
                data['v_R_yaw'].append(None)
                data['R_pitch'].append(None)
                data['R_dis'].append(None)
                data['yaw'].append(None)
                data['roll'].append(None)
                data['spd'].append(None)
            
            # 3. Parse State After Update line
            state_match = state_pattern.search(line)
            if state_match and current_time and data['time'] and data['time'][-1] == current_time:
                r_yaw, v_r_yaw, r_pitch, r_dis, yaw, roll, spd = map(float, state_match.groups())
                if data['R_yaw']:
                    data['R_yaw'][-1] = r_yaw
                    data['v_R_yaw'][-1] = v_r_yaw
                    data['R_pitch'][-1] = r_pitch
                    data['R_dis'][-1] = r_dis
                    data['yaw'][-1] = yaw
                    data['roll'][-1] = roll
                    data['spd'][-1] = spd
    
    return data

def plot_data(data, output_image='smalltarget_analysis.png'):
    """Plot 16 subplots with time as x-axis, plus 10-second zoomed subplots"""
    if not data['time']:
        print("No valid data found.")
        return

    time_axis = data['time']
    
    # Plot configurations: (row, column, data_key, y_label)
    plot_configs = [
        (0, 0, 'R_ypd_y', 'R_ypd Yaw (rad)'),
        (0, 1, 'R_ypd_p', 'R_ypd Pitch (rad)'),
        (0, 2, 'R_ypd_d', 'R_ypd Distance (m)'),
        (0, 3, 'ypr_y', 'ypr Yaw (rad)'),
        (1, 0, 'ypr_p', 'ypr Pitch (rad)'),
        (1, 1, 'ypr_r', 'ypr Roll (rad)'),
        (1, 2, 'Blade_ypd_y', 'Blade Yaw (rad)'),
        (1, 3, 'Blade_ypd_p', 'Blade Pitch (rad)'),
        (2, 0, 'Blade_ypd_d', 'Blade Distance (m)'),
        (2, 1, 'R_yaw', 'State R_yaw (rad)'),
        (2, 2, 'v_R_yaw', 'State v_R_yaw (rad/s)'),
        (2, 3, 'R_pitch', 'State R_pitch (rad)'),
        (3, 0, 'R_dis', 'State R_dis (m)'),
        (3, 1, 'yaw', 'State yaw (rad)'),
        (3, 2, 'roll', 'State roll (rad)'),
        (3, 3, 'spd', 'State Speed (rad/s)'),
    ]
    
    # Check data dimensions
    print(f"Time axis data points: {len(time_axis)}")
    for data_key in ['R_ypd_y', 'R_yaw', 'yaw', 'spd']:
        if data_key in data and data[data_key]:
            print(f"{data_key} data points: {len(data[data_key])}")
    
    # Create main figure with 4x4 subplots
    fig, axes = plt.subplots(4, 4, figsize=(28, 20), sharex=True)
    fig.suptitle('SmallTarget Module Parameter Time Series Analysis', fontsize=24, y=0.98)
    
    # Plot all subplots
    for idx, (row, col, data_key, ylabel) in enumerate(plot_configs):
        ax = axes[row, col]
        
        # Filter out None values
        y_data = data[data_key]
        valid_indices = [i for i, val in enumerate(y_data) if val is not None]
        
        if valid_indices:
            # Plot only valid data
            valid_time = [time_axis[i] for i in valid_indices]
            valid_values = [y_data[i] for i in valid_indices]
            
            # Plot main data
            line, = ax.plot(valid_time, valid_values, 'b-', linewidth=1.5, label=data_key)
            ax.set_ylabel(ylabel, fontsize=12)
            ax.grid(True, linestyle='--', alpha=0.5)
            ax.legend(loc='upper right', fontsize=9)
            
            # Auto-adjust Y axis
            y_min, y_max = min(valid_values), max(valid_values)
            y_range = y_max - y_min
            if y_range == 0:
                y_range = abs(y_min) * 0.1 if y_min != 0 else 1
            ax.set_ylim(y_min - 0.05*y_range, y_max + 0.05*y_range)
            
            # Add number of valid data points
            ax.text(0.02, 0.98, f'N={len(valid_values)}', transform=ax.transAxes,
                   fontsize=10, verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.7))
            
            # Create 10-second zoomed subplots
            create_10s_zoomed_plots(ax, valid_time, valid_values, data_key, ylabel, line)
            
        else:
            ax.text(0.5, 0.5, f'No valid data: {data_key}', transform=ax.transAxes,
                   ha='center', va='center', fontsize=12, color='red')
            ax.set_ylabel(ylabel, fontsize=12)
            ax.grid(True, linestyle='--', alpha=0.5)
    
    # Set bottom subplot X-axis labels and format
    for col in range(4):
        axes[3, col].set_xlabel('Time', fontsize=14)
        axes[3, col].xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
        axes[3, col].xaxis.set_major_locator(mdates.AutoDateLocator(maxticks=8))
        plt.setp(axes[3, col].xaxis.get_majorticklabels(), rotation=45, ha='right')
    
    # Adjust layout
    plt.tight_layout(rect=[0, 0.02, 1, 0.96])
    
    # Save the main figure
    plt.savefig(output_image, dpi=150, bbox_inches='tight')
    print(f"Main chart saved as: {output_image}")
    
    # Create separate zoomed figures for 10-second intervals
    create_10s_interval_figures(data, time_axis)
    
    plt.show()

def create_10s_zoomed_plots(main_ax, valid_time, valid_values, data_key, ylabel, main_line):
    """Create inset axes showing 10-second zoomed views"""
    if len(valid_time) < 2:
        return
    
    # Calculate time range
    total_time_span = (valid_time[-1] - valid_time[0]).total_seconds()
    
    # Create 10-second zoomed subplots as insets
    num_zooms = min(3, int(total_time_span // 10) + 1)  # Max 3 zoom windows
    
    for i in range(num_zooms):
        # Calculate time window
        start_offset = i * 10
        end_offset = (i + 1) * 10
        
        # Convert to datetime objects
        window_start = valid_time[0] + timedelta(seconds=start_offset)
        window_end = valid_time[0] + timedelta(seconds=end_offset)
        
        # Get data within window
        window_indices = [idx for idx, t in enumerate(valid_time) 
                         if window_start <= t <= window_end]
        
        if len(window_indices) < 2:
            continue
        
        # Create inset axes
        inset_ax = main_ax.inset_axes([0.05 + i*0.3, 0.05, 0.25, 0.25])
        
        # Plot zoomed data
        window_times = [valid_time[idx] for idx in window_indices]
        window_values = [valid_values[idx] for idx in window_indices]
        
        inset_ax.plot(window_times, window_values, 'r-', linewidth=1.2)
        inset_ax.grid(True, linestyle=':', alpha=0.4)
        inset_ax.set_title(f'{start_offset}-{end_offset}s', fontsize=8, pad=2)
        
        # Format time display
        inset_ax.xaxis.set_major_formatter(mdates.DateFormatter('%M:%S'))
        plt.setp(inset_ax.xaxis.get_majorticklabels(), rotation=45, fontsize=6)
        inset_ax.tick_params(axis='y', labelsize=6)
        
        # Add rectangle on main plot to show zoom area
        rect = Rectangle((window_start, min(window_values)), 
                        timedelta(seconds=10), 
                        max(window_values)-min(window_values),
                        linewidth=1, edgecolor='r', facecolor='none', alpha=0.5)
        main_ax.add_patch(rect)

def create_10s_interval_figures(data, time_axis):
    """Create separate figures for each 10-second interval"""
    if len(time_axis) < 2:
        return
    
    # Calculate total time span
    total_seconds = (time_axis[-1] - time_axis[0]).total_seconds()
    num_intervals = int(total_seconds // 10) + 1
    
    for interval_idx in range(num_intervals):
        # Create a new figure for this 10-second interval
        fig_interval, axes_interval = plt.subplots(4, 4, figsize=(24, 16), sharex=True)
        fig_interval.suptitle(f'SmallTarget Parameters - Interval {interval_idx*10}-{(interval_idx+1)*10} seconds', 
                             fontsize=20, y=0.98)
        
        # Define time window
        window_start = time_axis[0] + timedelta(seconds=interval_idx*10)
        window_end = time_axis[0] + timedelta(seconds=(interval_idx+1)*10)
        
        # Plot configurations
        plot_configs = [
            (0, 0, 'R_ypd_y', 'R_ypd Yaw (rad)'),
            (0, 1, 'R_ypd_p', 'R_ypd Pitch (rad)'),
            (0, 2, 'R_ypd_d', 'R_ypd Distance (m)'),
            (0, 3, 'ypr_y', 'ypr Yaw (rad)'),
            (1, 0, 'ypr_p', 'ypr Pitch (rad)'),
            (1, 1, 'ypr_r', 'ypr Roll (rad)'),
            (1, 2, 'Blade_ypd_y', 'Blade Yaw (rad)'),
            (1, 3, 'Blade_ypd_p', 'Blade Pitch (rad)'),
            (2, 0, 'Blade_ypd_d', 'Blade Distance (m)'),
            (2, 1, 'R_yaw', 'State R_yaw (rad)'),
            (2, 2, 'v_R_yaw', 'State v_R_yaw (rad/s)'),
            (2, 3, 'R_pitch', 'State R_pitch (rad)'),
            (3, 0, 'R_dis', 'State R_dis (m)'),
            (3, 1, 'yaw', 'State yaw (rad)'),
            (3, 2, 'roll', 'State roll (rad)'),
            (3, 3, 'spd', 'State Speed (rad/s)'),
        ]
        
        for row, col, data_key, ylabel in plot_configs:
            ax = axes_interval[row, col]
            y_data = data[data_key]
            
            # Get valid indices within time window
            valid_indices = []
            for i, (t, val) in enumerate(zip(time_axis, y_data)):
                if val is not None and window_start <= t <= window_end:
                    valid_indices.append(i)
            
            if len(valid_indices) >= 2:
                window_times = [time_axis[i] for i in valid_indices]
                window_values = [y_data[i] for i in valid_indices]
                
                ax.plot(window_times, window_values, 'g-', linewidth=2)
                ax.set_ylabel(ylabel, fontsize=10)
                ax.grid(True, linestyle='--', alpha=0.5)
                ax.set_title(f'{data_key} ({len(window_values)} pts)', fontsize=9)
                
                # Set time format - 修复这里：移除 [:10] 切片
                if row == 3:
                    ax.set_xlabel('Time', fontsize=10)
                    ax.xaxis.set_major_formatter(mdates.DateFormatter('%M:%S'))
                    plt.setp(ax.xaxis.get_majorticklabels(), rotation=45, fontsize=8)
            else:
                ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center', 
                       transform=ax.transAxes, fontsize=9, color='gray')
                ax.set_ylabel(ylabel, fontsize=10)
                ax.grid(True, linestyle='--', alpha=0.3)
        
        plt.tight_layout(rect=[0, 0.02, 1, 0.96])
        
        # Save the interval figure
        interval_filename = f'smalltarget_interval_{interval_idx*10}_{(interval_idx+1)*10}s.png'
        plt.savefig(interval_filename, dpi=150, bbox_inches='tight')
        print(f"Interval chart saved as: {interval_filename}")
        plt.close(fig_interval)

if __name__ == '__main__':
    # Replace with your actual log file path
    log_file_path = '/home/killer_machine/WRITE/build/logs/2026-03-13_11-27-15.log'
    
    print("Parsing log file...")
    parsed_data = parse_log_file(log_file_path)
    
    print(f"Successfully parsed {len(parsed_data['time'])} time points.")
    
    # Check data points for each parameter
    print("\nData points statistics for each parameter:")
    for key in parsed_data:
        if isinstance(parsed_data[key], list):
            valid_count = sum(1 for val in parsed_data[key] if val is not None)
            print(f"{key}: {len(parsed_data[key])} total points ({valid_count} valid)")
    
    print("Generating charts...")
    
    # Generate charts
    plot_data(parsed_data, output_image='smalltarget_analysis.png')