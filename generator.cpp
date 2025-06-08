
/**
 *
 * _____  _____  __                _           _ _                       
 * |  ___||  _  |/ _|              | |         | | |                      
 * |___ \ | |/' | |_ _ __  ___  ___| |__   __ _| | | ___ _ __   __ _  ___ 
 *     \ \|  /| |  _| '_ \/ __|/ __| '_ \ / _` | | |/ _ \ '_ \ / _` |/ _ \
 * /\__/ /\ |_/ / | | |_) \__ \ (__| | | | (_| | | |  __/ | | | (_| |  __/
 * \____/  \___/|_| | .__/|___/\___|_| |_|\__,_|_|_|\___|_| |_|\__, |\___|
 *                  | |                                         __/ |     
 *                  |_|                                        |___/      
 *
 * @file generator.cpp
 * @brief Simulates a camera by generating and saving random images at a fixed frame rate.
 * 
 * This module creates a producer thread that generates images at a specified FPS for a
 * given duration and multiple consumer threads that save these images to disk.
 */
#include <iostream>
#include <atomic>
#include <pthread.h>
#include "modules/SafetyQueue.h"
#include <chrono>
#include <thread>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include "./modules.h"

static pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queueCond = PTHREAD_COND_INITIALIZER;
static bool producerDone = false;
static bool timedOut = false;
static std::atomic<int> savedFrames{0};
static std::atomic<float> generationTime{0};
static std::atomic<float> saveTime{0};
static std::atomic<float> qTime{0};
static std::atomic<int> qCounter{0};
SafetyQueue q;
using namespace std;

/**
 * @struct Requirements
 * @brief Configuration parameters for image generation and processing.
 */
struct Requirements {
    int imageWidth;
    int imageHeight;
    int frames;
    int num_threads;
    int duration_minutes;
    std::string image_format;
};

/**
 * @struct Consumer_Args
 * @brief Arguments passed to each consumer thread.
 */
struct Consumer_Args {
    int thread_id;
    Requirements* req;
};


/**
 * @brief Generates a random color image of specified dimensions.
 *
 * @param w Image width in pixels.
 * @param h Image height in pixels.
 * @return Randomly generated cv::Mat image.
 */
cv::Mat generateRandomImage(int w, int h) {
    cv::Mat img(h, w, CV_8UC3);
    cv::randu(img, cv::Scalar::all(0), cv::Scalar::all(255));
    return img;
}

/**
 * @brief Producer thread function that generates images at a fixed FPS.
 *
 * This function runs until the specified duration elapses or a timeout flag is set.
 * It pushes generated images into a shared queue for consumers to process.
 *
 * Added debugging prints for generation time and queue size.
 *
 * @param arg Pointer to Requirements structure.
 * @return nullptr upon completion.
 */
void* producer(void* arg) {

    Requirements* req = static_cast<Requirements*>(arg);
    const double fps = req->frames;
    const auto framePeriod = std::chrono::duration<double>(1.0 / fps);
    auto startTime = std::chrono::high_resolution_clock::now();
    // auto endTime = startTime + std::chrono::minutes(req->duration_minutes);
    auto endTime = startTime + std::chrono::seconds(10); // For testing, set to 10 seconds

    int frame_id = 0;

    cv::Mat permanentImage = generateRandomImage(req->imageWidth, req->imageHeight);

    while (std::chrono::high_resolution_clock::now() < endTime) {
        auto loopStart = std::chrono::high_resolution_clock::now();

        // Check timeout under lock
        pthread_mutex_lock(&queueMutex);
        if (timedOut) {
            pthread_mutex_unlock(&queueMutex);
            break;
        }
        pthread_mutex_unlock(&queueMutex);

        // Time how long it takes to generate the image
        auto genStart = std::chrono::high_resolution_clock::now();
        cv::Mat img = permanentImage;
        auto genEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> genElapsed = genEnd - genStart;
        generationTime = generationTime + genElapsed.count();
        std::cout << "[Producer] frame " << frame_id;

        img_data data{ frame_id, img };
        frame_id++;

        // Measure and apply sleep if needed
        auto frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = frameEnd - loopStart;
        if (elapsed < framePeriod) {
            auto sleepTime = framePeriod - elapsed;
            std::cout << "[Producer] sleeping for " 
                      << std::chrono::duration<double, std::milli>(sleepTime).count()
                      << " ms to maintain " << fps << " fps\n";
            std::this_thread::sleep_for(sleepTime);
        }

        // Push to queue (locking moves inside SafetyQueue::push)
        auto startQ = std::chrono::high_resolution_clock::now();
        q.push(data);
        auto endQ = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsedQ = endQ - startQ;
        qTime = qTime + std::chrono::duration<double, std::milli>(elapsedQ).count();
        qCounter++;
        std::cout << "[Producer] queue push time: " 
                  << std::chrono::duration<double, std::milli>(elapsedQ).count() 
                  << " ms\n";

        // Signal consumers that a new item is available
        pthread_mutex_lock(&queueMutex);
        size_t currentSize = q.size();
        std::cout << "[Producer] queued image " << data.id
                  << ", queue size = " << currentSize << "\n";
        pthread_cond_signal(&queueCond);
        pthread_mutex_unlock(&queueMutex);
    }

    pthread_mutex_lock(&queueMutex);
    producerDone = true;
    pthread_cond_broadcast(&queueCond);
    pthread_mutex_unlock(&queueMutex);

    double totalSeconds = req->duration_minutes * 60.0;
    double effectiveFps = static_cast<double>(frame_id) / totalSeconds;
    std::cout << "[Producer] Finished. Effective generation fps: " 
              << effectiveFps << "\n";
    return nullptr;
}

/**
 * @brief Consumer thread function that saves images from the queue to disk.
 *
 * Each consumer waits for images to become available, then writes them to jpg files.
 *
 * Added debugging prints for save time and queue state.
 *
 * @param arg Pointer to Consumer_Args structure containing thread info.
 * @return nullptr upon completion.
 */
void* consumer(void* arg) {
    Consumer_Args* cargs = static_cast<Consumer_Args*>(arg);
    Requirements* req = cargs->req;
    int tid = cargs->thread_id;

    while (true) {
        pthread_mutex_lock(&queueMutex);
        while (q.empty() && !producerDone && !timedOut) {
            pthread_cond_wait(&queueCond, &queueMutex);
        }
        if (q.empty() && (producerDone || timedOut)) {
            pthread_mutex_unlock(&queueMutex);
            break;
        }
        // Safely retrieve front and pop under the same lock
        img_data item = q.front();
        q.pop();
        int remaining = q.size();
        pthread_mutex_unlock(&queueMutex);

        // Time how long it takes to save the image
        auto saveStart = std::chrono::high_resolution_clock::now();
        string filename = "../out/random_image_" + to_string(item.id + 1) + "." + req->image_format;
        bool ok = cv::imwrite(filename, item.img);
        auto saveEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> saveElapsed = saveEnd - saveStart;
        saveTime = saveTime + saveElapsed.count();

        if (!ok) {
            std::cerr << "[Consumer " << tid << "] failed to save " << filename << "\n";
        } else {
            savedFrames++;
            cout << "[Consumer " << tid << "] saved " << filename
                 << ", save time: " << saveElapsed.count() << " ms"
                 << ", queue size = " << remaining << "\n";
        }
    }
    return nullptr;
}

/**
 * @brief Entry point for the camera simulation.
 *
 * Initializes shared state, creates producer and consumer threads,
 * and waits for their completion.
 *
 * @param frames Frames per second to generate.
 * @param minutes Duration in minutes for image generation.
 * @param num_threads Number of consumer threads.
 * @return 0 on success.
 */
int main_generator(int width, int height, std::string image_format, int frames, int minutes, int num_threads) {
    Requirements* req = new Requirements{width, height, frames, num_threads, minutes, image_format};
    Consumer_Args* args = new Consumer_Args[num_threads];

    // Define queue's properties
    q.maxSize = 15;
    q.queueMutex = queueMutex;
    
    // Create threads
    pthread_t threads[num_threads];
    pthread_create(&threads[0], nullptr, producer, req);

    for (int i = 1; i < num_threads; ++i) {
        args[i].thread_id = i;
        args[i].req = req;
        pthread_create(&threads[i], nullptr, consumer, (void*)&args[i]);
    }

    // Wait for threads to finish
    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], nullptr);
    }

    int totalFrames = savedFrames.load();
    std::cout << "[Main] Average consumer fps " 
              << (totalFrames / (req->duration_minutes * 60)) << "\n";

    // print q stats
    cout << "[Main] Queue stats: "
        << "Total frames saved: " << totalFrames << " frames"
        << ", Average generation time: " << generationTime.load()/totalFrames << " miliseconds"
        << ", Average save time: " << saveTime.load()/totalFrames << " miliseconds"
        << ", Total queue time: " << qTime.load() << " miliseconds"
        << ", Queue average: " << qTime.load()/qCounter.load() << " ms";

    delete[] args;
    delete req;
    std::cout << "\n[Main] Program finished after " << minutes << " minutes.\n";
    return 0;
}
