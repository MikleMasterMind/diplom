import sys
import matplotlib.pyplot as plt
import numpy as np
import math

def calculate_quartile_stats(filename):
    """Вычисляет 1-й квартиль, медиану и 3-й квартиль Goodput из файла"""
    values = []
    
    with open(filename, 'r') as f:
        lines = f.readlines()
        
    for line in lines[1:]:
        if line.strip():
            parts = line.split()
            if len(parts) >= 2:
                try:
                    goodput = float(parts[1])
                    values.append(goodput)
                except ValueError:
                    continue
    
    if len(values) > 0:
        return {
            'q1': np.percentile(values, 25),
            'median': np.percentile(values, 50),
            'q3': np.percentile(values, 75),
            'values': values
        }
    else:
        return {'q1': 0.0, 'median': 0.0, 'q3': 0.0, 'values': []}

def main():
    num_files = int(sys.stdin.readline().strip())
    
    filenames = []
    for _ in range(num_files):
        filenames.append(sys.stdin.readline().strip())
    
    output_filename = sys.stdin.readline().strip()
    
    stats = []
    for filename in filenames:
        stat = calculate_quartile_stats(filename)
        stats.append(stat)
    
    # Рассчитываем количество строк (по 2 графика в строке)
    nrows = math.ceil(num_files / 2)
    ncols = 2
    
    # Создаём сетку с 2 столбцами
    fig, axes = plt.subplots(nrows, ncols, figsize=(12, 6 * nrows))
    
    # Если только один подграфик, делаем axes двумерным массивом для удобства
    if nrows == 1 and ncols == 1:
        axes = np.array([[axes]])
    elif nrows == 1:
        axes = axes.reshape(1, -1)
    elif ncols == 1:
        axes = axes.reshape(-1, 1)
    
    # Заполняем подграфики
    for i, (stat, ax) in enumerate(zip(stats, axes.flatten())):
        if len(stat['values']) > 0:
            ax.boxplot(stat['values'], vert=True, patch_artist=True,
                      whiskerprops=dict(color='blue'),
                      capprops=dict(color='blue'),
                      medianprops=dict(color='red', linewidth=15),
                      flierprops=dict(marker='o', markerfacecolor='gray', markersize=4))
        
        ax.set_title(f'Эксперимент {i+1}')
        ax.set_ylabel('Goodput (Mbps)')
        ax.grid(True, alpha=0.3, axis='y')
        ax.set_xticklabels([''])
        
        # Устанавливаем нижнюю границу как q3 / 2
        q3_val = stat['q3']
        if q3_val > 0:
            ax.set_ylim(bottom=q3_val * 0.9)
    
    # Скрываем неиспользуемые подграфики
    for j in range(len(stats), nrows * ncols):
        axes.flatten()[j].set_visible(False)
    
    # Общая настройка
    plt.tight_layout()
    plt.savefig(output_filename, dpi=100)
    plt.close()

if __name__ == "__main__":
    main()
