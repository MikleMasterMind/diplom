import matplotlib.pyplot as plt

def main():
    # Количество файлов
    n = int(input().strip())
    
    data_list = []
    labels = []
    xlabel = ""
    ylabel = ""
    
    for i in range(n):
        # Имя файла
        filename = input().strip()
        # Описание для легенды
        description = input().strip()
        
        labels.append(description)
        
        # Чтение данных из файла
        times = []
        goodputs = []
        with open(filename, 'r') as f:
            lines = f.readlines()
            # Из первого файла берём заголовок для осей
            if i == 0 and len(lines) > 0:
                header = lines[0].strip()
                # Убираем символ # и разбиваем
                if header.startswith('#'):
                    header = header[1:]
                parts = header.split()
                if len(parts) >= 2:
                    xlabel = parts[0]
                    ylabel = parts[1]
            
            # Читаем данные (со 2-й строки, если есть заголовок)
            start_line = 1 if len(lines) > 0 and '#' in lines[0] else 0
            for line in lines[start_line:]:
                if line.strip():
                    t, g = line.split()
                    times.append(float(t))
                    goodputs.append(float(g))
        data_list.append((times, goodputs))
    
    # Название файла для сохранения
    output_filename = input().strip()
    
    # Построение графика
    plt.figure(figsize=(10, 6))
    for (times, goodputs), label in zip(data_list, labels):
        plt.plot(times, goodputs, label=label)
    
    # Используем заголовки из первого файла
    plt.xlabel(xlabel if xlabel else 'X')
    plt.ylabel(ylabel if ylabel else 'Y')
    plt.legend()
    plt.grid(True)
    
    # Сохраняем график в файл
    plt.savefig(output_filename, dpi=300, bbox_inches='tight')
    plt.close()

if __name__ == "__main__":
    main()
