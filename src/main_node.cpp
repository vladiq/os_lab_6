#include <iostream>
#include "zmq.hpp"
#include <string>
#include <vector>
#include <signal.h>
#include <sstream>
#include <set>
#include <algorithm>
#include <unordered_map>
#include "server_functions.h"

class IdTree {
public:
    IdTree() = default;

    ~IdTree() {
        delete_node(head_);
    }

    bool contains(int id) {
        TreeNode* temp = head_;
        while(temp != nullptr) {
            if (temp->id_ == id) {
                break;
            }
            if (id > temp->id_) {
                temp = temp->right;
            }
            if (id < temp->id_) {
                temp = temp->left;
            }
        }
        return temp != nullptr;
    }

    void insert(int id) {
        if (head_ == nullptr) {
            head_ = new TreeNode(id);
            return;
        }
        TreeNode* temp = head_;
        while(temp != nullptr) {
            if (id == temp->id_) {
                break;
            }
            if (id < temp->id_) {
                if (temp->left == nullptr) {
                    temp->left = new TreeNode(id);
                    break;
                }
                temp = temp->left;
            }
            if (id > temp->id_) {
                if (temp->right == nullptr) {
                    temp->right = new TreeNode(id);
                    break;
                }
                temp = temp->right;
            }
        }
    }

    void erase(int id) {
        TreeNode* prev_id = nullptr;
        TreeNode* temp = head_;
        while (temp != nullptr) {
            if (id == temp->id_) {
                if (prev_id == nullptr) {
                    head_ = nullptr;
                } else {
                    if (prev_id->left == temp) {
                        prev_id->left = nullptr;
                    } else {
                        prev_id->right = nullptr;
                    }
                }
                delete_node(temp);

            } else if (id < temp->id_) {
                prev_id = temp;
                temp = temp->left;
            } else if (id > temp->id_) {
                prev_id = temp;
                temp = temp->right;
            }
        }
    }

    void add_to_dictionary(int id, std::string name, int value) {
        TreeNode* neededNode = search(head_, id);
        neededNode->dictionary[name] = value;
    }

    void get_from_dictionary(int id, std::string name) {
        TreeNode* neededNode = search(head_, id);
        if (neededNode->dictionary.find(name) == neededNode->dictionary.end()) {
            std::cout << "'" << name << "' not found" << std::endl;
        } else {
            std::cout << neededNode->dictionary[name] << std::endl;
        }
    }

    std::vector<int> get_nodes() const {
        std::vector<int> result;
        get_nodes(head_, result);
        return result;
    }

private:
    struct TreeNode {
        TreeNode(int id) : id_(id) {}
        int id_;
        TreeNode* left = nullptr;
        TreeNode* right = nullptr;
        std::unordered_map<std::string, int> dictionary;
    };

    TreeNode* search(TreeNode* root, int id) {
        if (root == nullptr || root->id_ == id) {
            return root;
        }

        if (root->id_ < id) {
            return search(root->right, id);
        }

        return search(root->left, id);
    }

    void get_nodes(TreeNode* node, std::vector<int>& v) const {
        if (node == nullptr) {
            return;
        }
        get_nodes(node->left,v);
        v.push_back(node->id_);
        get_nodes(node->right, v);
    }

    void delete_node(TreeNode* node) {
        if (node == nullptr) {
            return;
        }
        delete_node(node->right);
        delete_node(node->left);
        delete node;
    }

    TreeNode* head_ = nullptr;
};

int main() {
    std::string command;
    IdTree ids;
    size_t child_pid = 0;
    int child_id = 0;
    zmq::context_t context(1);
    zmq::socket_t main_socket(context, ZMQ_REQ);
    int linger = 0;
    main_socket.setsockopt(ZMQ_SNDTIMEO, 2000);
    main_socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
    int port = bind_socket(main_socket);

    while (true) {
        std::cin >> command;
        if (command == "create") {
            size_t node_id;
            std::string result;
            std::cin >> node_id;
            if (child_pid == 0) {
                child_pid = fork();
                if (child_pid == -1) {
                    std::cout << "Unable to create the first node\n";
                    child_pid = 0;
                    exit(1);
                } else if (child_pid == 0) {
                    create_node(node_id, port);
                } else {
                    child_id = node_id;
                    send_message(main_socket,"pid");
                    result = recieve_message(main_socket);
                }
            } else {
                std::ostringstream msg_stream;
                msg_stream << "create " << node_id;
                send_message(main_socket, msg_stream.str());
                result = recieve_message(main_socket);
            }

            if (result.substr(0,2) == "Ok") {
                ids.insert(node_id);
            }
            std::cout << result << "\n";

        } else if (command == "remove") {
            if (child_pid == 0) {
                std::cout << "Error:Not found\n";
                continue;
            }
            size_t node_id;
            std::cin >> node_id;
            if (node_id == child_id) {
                kill(child_pid, SIGTERM);
                kill(child_pid, SIGKILL);
                child_id = 0;
                child_pid = 0;
                std::cout << "Ok\n";
                ids.erase(node_id);
                continue;
            }
            std::string message_string = "remove " + std::to_string(node_id);
            send_message(main_socket, message_string);

            std::string recieved_message = recieve_message(main_socket);
            if (recieved_message.substr(0, std::min<int>(recieved_message.size(), 2)) == "Ok") {
                ids.erase(node_id);
            }
            std::cout << recieved_message << "\n";

        } else if (command == "exec") {
            int id;
            std::cin >> id;

            char nameAndValueArr[256];
            std::cin.getline(nameAndValueArr, 256);
            std::string nameAndValue = nameAndValueArr;

            std::string message_string = "exec " + std::to_string(id);
            send_message(main_socket, message_string);

            std::string recieved_message = recieve_message(main_socket);
            if (recieved_message == "Node is available") {
                std::string name;
                int value;
                std::stringstream ss(nameAndValue);
                bool searchNeeded = true;
                for (int i = 1; i < 256; ++i) {
                    if (nameAndValueArr[i] == ' ') {
                        ss >> name;
                        ss >> value;
                        ids.add_to_dictionary(id, name, value);
                        std::cout << "Ok:" << id << std::endl;
                        searchNeeded = false;
                        break;
                    }
                }
                if (searchNeeded) {
                    ss >> name;
                    std::cout << "Ok:" << id << ": ";
                    ids.get_from_dictionary(id, name);
                }
            } else {
                std::cout << recieved_message << std::endl;
            }

        } else if (command == "pingall") {
            send_message(main_socket,"pingall");
            std::string recieved = recieve_message(main_socket);
            std::istringstream is;
            if (recieved.substr(0,std::min<int>(recieved.size(), 5)) == "Error") {
                is = std::istringstream("");
            } else {
                is = std::istringstream(recieved);
            }
            
            std::set<int> recieved_ids;
            int rec_id;
            while (is >> rec_id) {
                recieved_ids.insert(rec_id);
            }
            std::vector from_tree = ids.get_nodes();
            auto part_it = std::partition(from_tree.begin(), from_tree.end(), [&recieved_ids] (int a) {
                return recieved_ids.count(a) == 0;
            });
            if (part_it == from_tree.begin()) {
                std::cout << "Ok: -1\n";
            } else {
                std::cout << "Ok:";
                for (auto it = from_tree.begin(); it != part_it; ++it) {
                    std::cout << " " << *it;
                }
                std::cout << "\n";
            }

        } else if (command == "exit") {
            break;
        }

    }

    return 0;
}
