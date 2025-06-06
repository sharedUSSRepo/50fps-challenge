#include <iostream>
#include <pthread.h>
#include <queue>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

using namespace std;

/**
 * @struct img_data
 * @brief Container for image data and its identifier.
 */
struct img_data {
    int id;
    cv::Mat img;
};

/**
 * @brief SafetyQueue class that wraps a standard queue with thread-safe operations and limited size.
 *
 * @param maxSize Maximum number of items allowed in the queue.
 * @param queueMutex Mutex for locking access to the queue to others threads.
 */
class SafetyQueue {
    public:
        queue<img_data> q;
        int maxSize;
        pthread_mutex_t queueMutex;

        void push(const img_data& data) {
            pthread_mutex_lock(&queueMutex);
            if (q.size() >= maxSize) { // Example size limit
                std::cerr << "[SafetyQueue] Queue is full, dropping frame " << data.id << "\n";
                q.pop(); // Drop the oldest frame
                pthread_mutex_unlock(&queueMutex);
                return;
            }
            q.push(data);
            pthread_mutex_unlock(&queueMutex);
        }

        void pop() {
            if (!q.empty()) {
                q.pop();
            }
        }

        img_data front() {
            img_data frontData;
            if (!q.empty()) {
                frontData = q.front();
            } else {
                frontData.id = -1; // Indicate empty queue
                frontData.img = cv::Mat();
            }
            return frontData;
        }

        size_t size() {
            return q.size();
        }

        bool empty() {
            return q.empty();
        }
};
