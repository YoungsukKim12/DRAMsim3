import json

# Load JSON file and calculate the sum of total_energy, printing each value
def calculate_and_print_energies(file_path):
    with open(file_path, 'r', encoding='utf-8') as file:
        data = json.load(file)
    
    total_energy_sum = 0
    act_energy_sum = 0
    act_cmd_sum = 0
    read_cmd_sum = 0
    read_energy_sum = 0
    for key, item in data.items():
        read_cmd = item.get("num_read_cmds", 0)
        read_energy = item.get("read_energy", 0)
        act_cmd = item.get("num_act_cmds", 0)
        act_energy = item.get("act_energy", 0)
        total_energy = item.get("total_energy", 0)
        # cprint(f"Channel {key}: Total Energy = {energy}")
        read_energy_sum += read_energy
        read_cmd_sum += read_cmd
        act_cmd_sum += act_cmd
        act_energy_sum += act_energy
        total_energy_sum += total_energy
    
    print(f"RD CMD Sum: {read_cmd_sum}")
    print(f"RD Energy Sum: {read_energy_sum}")
    print(f"ACT CMD Sum: {act_cmd_sum}")
    print(f"ACT Energy Sum: {act_energy_sum}")
    print(f"Total Energy Sum: {total_energy_sum}")

# Replace 'your_file_path.json' with the actual JSON file path
file_path = 'PHBM.json'  # 여기에 JSON 파일 경로를 입력하세요.
calculate_and_print_energies(file_path)
