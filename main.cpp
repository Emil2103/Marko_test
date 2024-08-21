#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>

struct image {
    int width, height;
    int format; // 0=GRAY   1=RGB   2=BGR
    uint8_t *data;
};

struct box {
    int x1, y1;
    int x2, y2;
    int type;   // 0=FACE   1=GUN   2=MASK
};

struct frame {
    image img;
    std::vector<box> boxes;
};

//  функция преобразует формат изображения из RGB в BGR
bool rgb2bgr(image &img) {
    //Если на вход получаем не rgb изображение, то выводим false
    if(img.format != 1){
        return false;
    }
    
    uint8_t *ptr = img.data;
    
    for(int i = 0; i < img.height * img.width; ++i){
        std::swap(ptr[0], ptr[2]);
        ptr += 3;
    }
    img.format = 2;
    
    return true;
}

float calculate_iou(const box &b1, const box &b2){
    // Проверка на то, что области пересекаются
    int x_left = std::max(b1.x1, b2.x1);
    int x_right = std::min(b1.x2, b2.x2);
    int y_top = std::max(b1.y1, b2.y1);
    int y_bottom = std::min(b1.y2, b2.y2);
    if(x_left > x_right || y_bottom < y_top){
        return 0.0f;
    }
    
    //площадь пересечения
    int intersection = (x_right - x_left) * (y_bottom - y_top);
    int b1_area = (b1.x2 - b1.x1) * (b1.y2 - b1.y1);
    int b2_area = (b2.x2 - b2.x1) * (b2.y2 - b2.y1);
    
    return static_cast<float>(intersection) / (b1_area + b2_area - intersection);
}

//  функция очищает кадр, оставляя одну рамку для общих объектов
//  объект считается общим для двух box, если их IOU >= threshold
void frame_clean(frame& f, float threshold) {
    // создаем булевский вектор, равный размеру количества детектированных объектов
    std::vector<bool> keep(f.boxes.size(), true);
    
    for(size_t i = 0; i < f.boxes.size(); ++i){
        if(!keep[i]){
            continue;
        }
        for(size_t j = i+1; j < f.boxes.size(); ++j){
            //Если полученное значение больше threshold, то мы рассматриваем две рамки указывающие на один объект
            //Значение для второй рамки в булевском векторе устанавливаем False
            if(calculate_iou(f.boxes[i], f.boxes[j]) >= threshold){
                keep[j] = false;
            }
        }
    }
    //Достаем только оставшиеся рамки
    std::vector<box> cleaned_boxes;
    for(size_t i = 0; i < f.boxes.size(); ++i){
        if(keep[i]){
            cleaned_boxes.push_back(f.boxes[i]);
        }
    }
    f.boxes = cleaned_boxes;
}

//  функция объединяет обнаруженные объекты из двух кадров в один
//  объект считается общим для двух box, если их IOU >= threshold
//  гарантируется, что f1.img == f2.img
frame union_frames(frame& f1, frame& f2, float threshold) {
    frame result = f1; // Инициализация результирующего кадра как копия первого кадра

        for (const auto& box2 : f2.boxes) {
            bool merged = false;
            // Проверяем каждую рамку из result на пересечение с box2
            for (auto& box1 : result.boxes) {
                if (calculate_iou(box1, box2) >= threshold) {
                    // Если рамки пересекаются, считаем их одним объектом
                    // Обновляем box1, объединив его с box2 (усреднение координат)
                    box1.x1 = std::min(box1.x1, box2.x1);
                    box1.y1 = std::min(box1.y1, box2.y1);
                    box1.x2 = std::max(box1.x2, box2.x2);
                    box1.y2 = std::max(box1.y2, box2.y2);
                    merged = true;
                    break; // Выход из внутреннего цикла
                }
            }
            // Если box2 не был объединен с существующими рамками, добавляем его
            if (!merged) {
                result.boxes.push_back(box2);
            }
        }

        return result;
}


// ТЕСТЫ

void test_rgb2bgr() {
    uint8_t image_data[12] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
    image img = {2, 2, 1, image_data};
    
    assert(rgb2bgr(img) == true);
    assert(img.format == 2);
    
    uint8_t expected_data[12] = {0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255};
    for (int i = 0; i < 12; ++i) {
    assert(img.data[i] == expected_data[i]);
    }
    
    std::cout << "test_rgb2bgr passed!" << std::endl;
}

void test_calculate_iou() {
    box b1 = {0, 0, 2, 2, 0};
    box b2 = {1, 1, 3, 3, 0};
    box b3 = {4, 4, 6, 6, 0};
    
    assert(calculate_iou(b1, b2) == 1.0f / 7.0f);
    assert(calculate_iou(b1, b3) == 0.0f);
    
    std::cout << "test_calculate_iou passed!" << std::endl;
}

void test_frame_clean() {
    frame f;
    f.boxes.push_back({0, 0, 4, 4, 0});
    f.boxes.push_back({1, 1, 5, 5, 0});
    f.boxes.push_back({5, 5, 9, 9, 0});

    frame_clean(f, 0.3);

    assert(f.boxes.size() == 2);

    std::cout << "test_frame_clean passed!" << std::endl;
}

void test_union_frames() {
    frame f1, f2;
    f1.boxes.push_back({0, 0, 4, 4, 0});
    f1.boxes.push_back({5, 5, 9, 9, 0});

    f2.boxes.push_back({2, 2, 6, 6, 0});
    f2.boxes.push_back({10, 10, 14, 14, 0});

    frame result = union_frames(f1, f2, 0.1);

    assert(result.boxes.size() == 3);

    std::cout << "test_union_frames passed!" << std::endl;
}


int main() {
    test_rgb2bgr();
    test_calculate_iou();
    test_frame_clean();
    test_union_frames();
    return 0;
}

