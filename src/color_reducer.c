#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define K_CLUSTER_ITERATIONS 8
// the higher rarity tolerance, the more uncommon a color can be
#define RARITY_TOLERANCE 16
#define MAX_RANDOM_OFFSET 5
#define GET_RANDOM_OFFSET rand() % (MAX_RANDOM_OFFSET * 2) - MAX_RANDOM_OFFSET
#define MIN_STDEV 2

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
	uint8_t closest_color;
} pixel;

void update_color_sums(uint32_t* color_sums, uint8_t i, pixel* p) {
	color_sums[i * 4 + 0] += p->r;
	color_sums[i * 4 + 1] += p->g;
	color_sums[i * 4 + 2] += p->b;
	color_sums[i * 4 + 3] += p->a;
}

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("Usage: color_reducer image_file num_colors\n");
		return 1;
	}

    	int width, height, bpp;

	uint8_t* rgb_image = stbi_load(argv[1], &width, &height, &bpp, 4);

	if (!rgb_image) {
		printf("Image %s does not exist.\n", argv[1]);
		return 2;
	}

	printf("width=%d,height=%d,bpp=%d\n", width, height, bpp);
	if (bpp != 3 && bpp != 4) {
		printf("Bytes per pixel must equal 3 (standard RGB) or 4 (RGBA)\n");
		return 3;
	}

	uint8_t color_num = atoi(argv[2]);
	if (color_num <= 0 || atoi(argv[2]) >= 256) {
		printf("Please enter an integer in [1, 255] for num_colors\n");
		return 4;
	}

	size_t pixel_count = width * height;

	pixel* pixels = malloc(sizeof(pixel) * pixel_count);
	for (size_t i = 0; i < pixel_count; i++) {
		pixels[i].r = rgb_image[i * 4 + 0];
		pixels[i].g = rgb_image[i * 4 + 1];
		pixels[i].b = rgb_image[i * 4 + 2];
		if (bpp == 4) // get alpha if alpha is described in the image (i.e. png)
			pixels[i].a = rgb_image[i * 4 + 3];
		else 
			pixels[i].a = 0xFF;
	}

	pixel** rand_order_pixels = malloc(sizeof(pixel*) * pixel_count);

	for (size_t i = 0; i < pixel_count; i++) {
		rand_order_pixels[i] = &pixels[i];
	}

	int rnd;
	pixel* tmp;


	uint32_t* color_sums = calloc(color_num * 4, sizeof(*color_sums));
	size_t* pixels_per_group = calloc(color_num, sizeof(*pixels_per_group));

	for (size_t i = 0; i < pixel_count; i++) {
		rnd = rand() % (pixel_count - i);
		tmp = rand_order_pixels[rnd];
		rand_order_pixels[rnd] = rand_order_pixels[pixel_count - i - 1];
		rand_order_pixels[pixel_count - i - 1] = tmp;

		update_color_sums(color_sums, i % color_num, tmp);
		pixels_per_group[i % color_num]++;
	}

	free(rand_order_pixels);

	float* avg_pixels_f = malloc(sizeof(*avg_pixels_f) * color_num * 4);

	printf("ITERATION -1:\n");
	for (size_t i = 0; i < color_num * 4; i++) {
		avg_pixels_f[i] = ((float) color_sums[i]) / pixels_per_group[i / 4];
		if (i % 4 == 0)
			printf("Color %lu: (", i / 4);
		printf("%f", avg_pixels_f[i]);
		if (i % 4 == 3)
			printf(")\n");
		else
			printf(", ");
	}
	printf("\n");

	float distance;
	float smallest_distance;
	float* distances = malloc(sizeof(*distances) * pixel_count);
	uint8_t* colors_sorted = malloc(sizeof(*colors_sorted) * color_num);
	uint8_t most_popular_color;
	uint8_t most_popular_color_index;
	size_t most_popular_color_count;
	size_t minimum_pixel_count = pixel_count / color_num / RARITY_TOLERANCE;
	float* color_stdevs = malloc(sizeof(*color_stdevs) * color_num);

	printf("Minimum pixel count: %lu\n", minimum_pixel_count);
	for (size_t i = 0; i < K_CLUSTER_ITERATIONS; i++) {
		for (size_t j = 0; j < color_num; j++) {
			for (size_t k = j * 4; k < j * 4 + 4; k++) {
				color_sums[k] = 0;
			}
			pixels_per_group[j] = 0;
			color_stdevs[j] = 0;
		}

		for (size_t j = 0; j < pixel_count; j++) {
			tmp = &pixels[j];

			smallest_distance = (float)pixel_count;
			for (uint8_t k = 0; k < color_num; k++) {
				distance = sqrt((tmp->r - avg_pixels_f[k * 4 + 0]) * (tmp->r - avg_pixels_f[k * 4 + 0]) +
					 	(tmp->g - avg_pixels_f[k * 4 + 1]) * (tmp->g - avg_pixels_f[k * 4 + 1]) +
					 	(tmp->b - avg_pixels_f[k * 4 + 2]) * (tmp->b - avg_pixels_f[k * 4 + 2]) +
					 	(tmp->a - avg_pixels_f[k * 4 + 3]) * (tmp->a - avg_pixels_f[k * 4 + 3]));
				if (distance < smallest_distance) {
					smallest_distance = distance;
					tmp->closest_color = k;
				}
			}
			distances[j] = smallest_distance;
			update_color_sums(color_sums, tmp->closest_color, tmp);
			pixels_per_group[tmp->closest_color]++;
		}

		printf("ITERATION %lu:\n", i);
		for (size_t j = 0; j < color_num * 4; j++) {
			avg_pixels_f[j] = ((float) color_sums[j]) / pixels_per_group[j / 4];
			if (j % 4 == 0)
				printf("Color %lu: (", j / 4);
			printf("%f", avg_pixels_f[j]);
			if (j % 4 == 3)
				printf(") (%lu pixels nearest)\n", pixels_per_group[j / 4]);
			else
				printf(", ");
		}
		printf("\n");

		for (size_t j = 0; j < pixel_count; j++) {
			tmp = &pixels[j];

			color_stdevs[tmp->closest_color] += sqrt((tmp->r - avg_pixels_f[tmp->closest_color * 4 + 0]) * (tmp->r - avg_pixels_f[tmp->closest_color * 4 + 0]) +
				 				 (tmp->g - avg_pixels_f[tmp->closest_color * 4 + 1]) * (tmp->g - avg_pixels_f[tmp->closest_color * 4 + 1]) +
				 				 (tmp->b - avg_pixels_f[tmp->closest_color * 4 + 2]) * (tmp->b - avg_pixels_f[tmp->closest_color * 4 + 2]) +
				 				 (tmp->a - avg_pixels_f[tmp->closest_color * 4 + 3]) * (tmp->a - avg_pixels_f[tmp->closest_color * 4 + 3]));
		}

		for (uint8_t j = 0; j < color_num; j++) {
			color_stdevs[j] = sqrt(color_stdevs[j] / pixels_per_group[j]);
			printf("Color %d stdev: %f\n", j, color_stdevs[j]);
		}

		for (size_t j = 0; j < color_num; j++) {
			colors_sorted[j] = j;
		}

		for (size_t j = 0; j < color_num; j++) {
			most_popular_color_count = 0;
			for (size_t k = 0; k < color_num - j; k++) {
				if (pixels_per_group[colors_sorted[k]] >= most_popular_color_count) {
					most_popular_color = colors_sorted[k];
					most_popular_color_index = k;
					most_popular_color_count = pixels_per_group[colors_sorted[k]];
				}
			}
			colors_sorted[most_popular_color_index] = colors_sorted[color_num - j - 1];
			colors_sorted[color_num - j - 1] = most_popular_color;
		}

		// for the first color that has no pixels closest to it, move its coordinates near the most populous color (while maintaining the same alpha value)
		most_popular_color_index = color_num - 1;
		most_popular_color = colors_sorted[most_popular_color_index];
		for (size_t j = 0; j < color_num; j++) {
			while (color_stdevs[most_popular_color] < MIN_STDEV && most_popular_color_index > 0) {
				most_popular_color_index--;
				most_popular_color = colors_sorted[most_popular_color_index];
			}
				
			if (pixels_per_group[most_popular_color] < minimum_pixel_count) break;

			if (pixels_per_group[j] < minimum_pixel_count) {
				printf("Moving Color %lu near Color %d...\n", j, most_popular_color);

				avg_pixels_f[j * 4 + 0] = avg_pixels_f[most_popular_color * 4 + 0] + GET_RANDOM_OFFSET;
				avg_pixels_f[j * 4 + 1] = avg_pixels_f[most_popular_color * 4 + 1] + GET_RANDOM_OFFSET;
				avg_pixels_f[j * 4 + 2] = avg_pixels_f[most_popular_color * 4 + 2] + GET_RANDOM_OFFSET;
				avg_pixels_f[j * 4 + 3] = avg_pixels_f[most_popular_color * 4 + 3];
				
				most_popular_color_index--;
				most_popular_color = colors_sorted[most_popular_color_index];
				
				if (pixels_per_group[most_popular_color] < minimum_pixel_count) break;
			}
		}
		printf("\n");
	}

	free(colors_sorted);

	free(pixels_per_group);
	free(color_sums);
	free(distances);
	free(color_stdevs);

	pixel* avg_pixels = malloc(sizeof(*avg_pixels) * color_num);
	
	for (size_t i = 0; i < color_num; i++) {
		avg_pixels[i].r = (uint8_t) avg_pixels_f[i * 4 + 0];
		avg_pixels[i].g = (uint8_t) avg_pixels_f[i * 4 + 1];
		avg_pixels[i].b = (uint8_t) avg_pixels_f[i * 4 + 2];
		avg_pixels[i].a = (uint8_t) avg_pixels_f[i * 4 + 3];
	}
	
	free(avg_pixels_f);

	for (size_t i = 0; i < pixel_count; i++) {
		rgb_image[i * bpp + 0] = avg_pixels[pixels[i].closest_color].r;
		rgb_image[i * bpp + 1] = avg_pixels[pixels[i].closest_color].g;
		rgb_image[i * bpp + 2] = avg_pixels[pixels[i].closest_color].b;
		if (bpp == 4)
			rgb_image[i * bpp + 3] = avg_pixels[pixels[i].closest_color].a;
	}

	free(avg_pixels);
	free(pixels);

	char out_file_name[256];
	for (size_t i = strlen(argv[1]); i > 0; i--) {
		if (argv[1][i] == '.') {
			argv[1][i] = '\0';
		}
		if (argv[1][i] == '/' || argv[1][i] == '\\') {
			argv[1] += i + 1;
			break;
		}
	}
	sprintf(out_file_name, "out/%s-%d.png", argv[1], color_num);
	printf("%s\n", out_file_name);
	stbi_write_png(out_file_name, width, height, bpp, rgb_image, width*bpp);

    	stbi_image_free(rgb_image);

    	return 0;
}
