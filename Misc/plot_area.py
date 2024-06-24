import matplotlib.pyplot as plt

# Function to read coordinates from a file
def read_coordinates(filename):
    coordinates = []
    with open(filename, 'r') as file:
        for line in file:
            _, _, x, y = line.strip().split()
            coordinates.append((float(x[2:]), float(y[2:])))
    return coordinates

# Function to plot the coordinates with direction arrows
def plot_coordinates_with_directions(coordinates, color="bo"):
    for i in range(len(coordinates) - 1):
        x1, y1 = coordinates[i]
        x2, y2 = coordinates[i + 1]
        
        # Plot the points
        plt.plot(y1, x1, color)  # 'bo' means blue color, circle marker
        plt.plot(y2, x2, color)
        
        # Plot the arrow
        plt.arrow(y1, x1, y2 - y1, x2 - x1, head_width=0, head_length=0, fc='red', ec='red')
    
    # Plot the last point
    x_last, y_last = coordinates[-1]
    plt.plot(y_last, x_last, color)

# Read coordinates from file
filename = 'coordinates.txt'
filename2 = 'coordinates2.txt'

plt.figure(figsize=(10, 8))
plot_coordinates_with_directions(read_coordinates(filename))
plot_coordinates_with_directions(read_coordinates(filename2), "go")
plt.xlabel('Y')
plt.ylabel('X')
plt.title('Plot of Coordinates with Directions')
plt.grid(True)
plt.show()